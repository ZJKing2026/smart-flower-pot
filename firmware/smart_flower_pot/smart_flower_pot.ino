#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <BH1750.h>
#include <U8g2lib.h>

constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;
constexpr uint8_t SOIL_SENSOR_PIN = 1;
constexpr uint8_t RELAY_PIN = 10;
constexpr uint8_t BUZZER_PIN = 11;
constexpr uint8_t RELAY_ON = LOW;
constexpr uint8_t RELAY_OFF = HIGH;
constexpr uint8_t SHT30_ADDRESS = 0x44;
constexpr uint8_t BH1750_ADDRESS = 0x23;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr uint8_t ADC_RESOLUTION_BITS = 12;
constexpr uint16_t ADC_MAX_VALUE = 4095;
constexpr uint8_t SOIL_SAMPLE_COUNT = 20;
constexpr unsigned long SOIL_SAMPLE_DELAY_MS = 5;
constexpr unsigned long STARTUP_DELAY_MS = 1000;
constexpr unsigned long SENSOR_SAMPLE_INTERVAL_MS = 2000;
constexpr unsigned long MAX_PUMP_RUNTIME_MS = 3000;
constexpr unsigned long BUZZER_INTERVAL_MS = 2000;
constexpr unsigned long BUZZER_DURATION_MS = 200;
constexpr float MIN_TEMPERATURE_C = -40.0F;
constexpr float MAX_TEMPERATURE_C = 125.0F;
constexpr float MIN_HUMIDITY = 0.0F;
constexpr float MAX_HUMIDITY = 100.0F;
constexpr float LOW_LIGHT_THRESHOLD_LX = 50.0F;
constexpr uint16_t DRY_SOIL_THRESHOLD_ADC = 2600;
constexpr uint8_t ALERT_CONFIRM_COUNT = 3;

struct SensorData {
  float temperatureC;
  float humidity;
  float lightLux;
  uint16_t soilAdc;
  bool temperatureValid;
  bool humidityValid;
  bool lightValid;
  bool soilValid;
};

enum class AlertState : uint8_t {
  OK,
  LIGHT,
  SOIL,
  BOTH
};

Adafruit_SHT31 sht30(&Wire);
BH1750 lightMeter;
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

SensorData latestData = {};
AlertState currentAlertState = AlertState::OK;
bool sensorDataAvailable = false;
bool sht30Initialized = false;
bool bh1750Initialized = false;
bool pumpRunning = false;
bool lowLightAlertActive = false;
bool drySoilAlertActive = false;
bool buzzerPulseActive = false;
uint8_t lowLightFaultCount = 0;
uint8_t lowLightNormalCount = 0;
uint8_t drySoilFaultCount = 0;
uint8_t drySoilNormalCount = 0;
unsigned long pumpStartedAt = 0;
unsigned long lastSensorReadAt = 0;
unsigned long lastBuzzerPulseAt = 0;
unsigned long buzzerPulseStartedAt = 0;
unsigned long sampleNumber = 1;

/**
 * 获取告警状态的显示文本。
 *
 * @param state 当前告警状态。
 * @return 对应的英文状态文本。
 */
const char *alertStateText(AlertState state) {
  switch (state) {
    case AlertState::LIGHT:
      return "LIGHT";
    case AlertState::SOIL:
      return "SOIL";
    case AlertState::BOTH:
      return "BOTH";
    case AlertState::OK:
    default:
      return "OK";
  }
}

/**
 * 获取水泵状态的显示文本。
 *
 * @return 当前水泵状态文本。
 */
const char *pumpStateText() {
  return pumpRunning ? "ON" : "OFF";
}

/**
 * 初始化继电器和蜂鸣器为安全关闭状态。
 */
void initializeOutputs() {
  pinMode(RELAY_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  pinMode(BUZZER_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, HIGH);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
}

/**
 * 输出串口命令帮助信息。
 */
void printHelp() {
  Serial.println("Commands: W = water up to 3 seconds, S = stop, H = status.");
}

/**
 * 停止水泵并记录停止原因。
 *
 * @param reason 水泵停止原因。
 */
void stopPump(const char *reason) {
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pumpRunning = false;
  Serial.print("Pump: OFF, reason: ");
  Serial.println(reason);
}

/**
 * 启动一次受最长运行时间保护的手动浇水。
 */
void startPump() {
  if (pumpRunning) {
    Serial.println("Pump is already running. Command ignored.");
    return;
  }

  digitalWrite(RELAY_PIN, RELAY_ON);
  pumpRunning = true;
  pumpStartedAt = millis();
  Serial.println("Pump: ON, maximum runtime: 3 seconds.");
}

/**
 * 读取并校验 SHT30 温湿度数据。
 *
 * @param data 保存温湿度读数和有效状态的数据结构。
 */
void readSht30(SensorData &data) {
  data.temperatureValid = false;
  data.humidityValid = false;

  if (!sht30Initialized) {
    return;
  }

  data.temperatureC = sht30.readTemperature();
  data.humidity = sht30.readHumidity();
  data.temperatureValid = !isnan(data.temperatureC) &&
                          data.temperatureC >= MIN_TEMPERATURE_C &&
                          data.temperatureC <= MAX_TEMPERATURE_C;
  data.humidityValid = !isnan(data.humidity) &&
                       data.humidity >= MIN_HUMIDITY &&
                       data.humidity <= MAX_HUMIDITY;
}

/**
 * 读取并校验 BH1750 光照数据。
 *
 * @param data 保存光照读数和有效状态的数据结构。
 */
void readBh1750(SensorData &data) {
  data.lightValid = false;

  if (!bh1750Initialized) {
    return;
  }

  data.lightLux = lightMeter.readLightLevel();
  data.lightValid = !isnan(data.lightLux) && data.lightLux >= 0.0F;
}

/**
 * 采集多次土壤传感器 ADC 并计算平均值。
 *
 * @param data 保存土壤 ADC 读数和有效状态的数据结构。
 */
void readSoilAdc(SensorData &data) {
  uint32_t total = 0;
  bool samplesValid = true;

  for (uint8_t index = 0; index < SOIL_SAMPLE_COUNT; index++) {
    const int rawValue = analogRead(SOIL_SENSOR_PIN);
  if (rawValue < 0 || rawValue > ADC_MAX_VALUE) {
    samplesValid = false;
  } else {
      total += static_cast<uint16_t>(rawValue);
    }
    delay(SOIL_SAMPLE_DELAY_MS);
  }

  data.soilValid = samplesValid;
  if (samplesValid) {
    data.soilAdc = total / SOIL_SAMPLE_COUNT;
  }
}

/**
 * 读取一轮全部传感器数据。
 *
 * @return 当前传感器读数和有效状态。
 */
SensorData readSensors() {
  SensorData data = {};
  readSht30(data);
  readBh1750(data);
  readSoilAdc(data);
  return data;
}

/**
 * 更新单项告警的连续异常和恢复计数。
 *
 * @param measurementValid 本轮测量是否有效。
 * @param abnormal 本轮是否异常。
 * @param faultCount 连续异常计数。
 * @param normalCount 连续正常计数。
 * @param alertActive 稳定告警状态。
 */
void updateAlertFlag(
  bool measurementValid,
  bool abnormal,
  uint8_t &faultCount,
  uint8_t &normalCount,
  bool &alertActive
) {
  if (!measurementValid) {
    return;
  }

  if (abnormal) {
    normalCount = 0;
    if (faultCount < ALERT_CONFIRM_COUNT) {
      faultCount++;
    }
    if (faultCount >= ALERT_CONFIRM_COUNT) {
      alertActive = true;
    }
    return;
  }

  faultCount = 0;
  if (normalCount < ALERT_CONFIRM_COUNT) {
    normalCount++;
  }
  if (normalCount >= ALERT_CONFIRM_COUNT) {
    alertActive = false;
  }
}

/**
 * 根据传感器读数更新综合告警状态。
 *
 * @param data 当前传感器读数和有效状态。
 * @return 当前综合告警状态。
 */
AlertState updateAlertState(const SensorData &data) {
  updateAlertFlag(
    data.lightValid,
    data.lightValid && data.lightLux < LOW_LIGHT_THRESHOLD_LX,
    lowLightFaultCount,
    lowLightNormalCount,
    lowLightAlertActive
  );
  updateAlertFlag(
    data.soilValid,
    data.soilValid && data.soilAdc < DRY_SOIL_THRESHOLD_ADC,
    drySoilFaultCount,
    drySoilNormalCount,
    drySoilAlertActive
  );

  if (lowLightAlertActive && drySoilAlertActive) {
    return AlertState::BOTH;
  }
  if (lowLightAlertActive) {
    return AlertState::LIGHT;
  }
  if (drySoilAlertActive) {
    return AlertState::SOIL;
  }
  return AlertState::OK;
}

/**
 * 按告警状态执行非阻塞蜂鸣器短响。
 *
 * @param state 当前综合告警状态。
 */
void updateBuzzer(AlertState state) {
  const unsigned long now = millis();

  if (state == AlertState::OK) {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerPulseActive = false;
    return;
  }

  if (buzzerPulseActive) {
    if (now - buzzerPulseStartedAt >= BUZZER_DURATION_MS) {
      digitalWrite(BUZZER_PIN, HIGH);
      buzzerPulseActive = false;
    }
    return;
  }

  if (now - lastBuzzerPulseAt >= BUZZER_INTERVAL_MS) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerPulseActive = true;
    buzzerPulseStartedAt = now;
    lastBuzzerPulseAt = now;
  }
}

/**
 * 将当前传感器、告警和水泵状态输出到串口。
 */
void printSystemStatus() {
  Serial.println("System status:");

  Serial.print("Temperature: ");
  if (sensorDataAvailable && latestData.temperatureValid) {
    Serial.printf("%.1f C\n", latestData.temperatureC);
  } else {
    Serial.println("invalid");
  }

  Serial.print("Humidity: ");
  if (sensorDataAvailable && latestData.humidityValid) {
    Serial.printf("%.1f %%\n", latestData.humidity);
  } else {
    Serial.println("invalid");
  }

  Serial.print("Light: ");
  if (sensorDataAvailable && latestData.lightValid) {
    Serial.printf("%.1f lx\n", latestData.lightLux);
  } else {
    Serial.println("invalid");
  }

  Serial.print("Soil ADC: ");
  if (sensorDataAvailable && latestData.soilValid) {
    Serial.println(latestData.soilAdc);
  } else {
    Serial.println("invalid");
  }

  Serial.print("Alert: ");
  Serial.println(alertStateText(currentAlertState));
  Serial.print("Pump: ");
  Serial.println(pumpStateText());
}

/**
 * 输出一轮自动采样的串口数据。
 */
void printSampleData() {
  Serial.printf("Sample #%lu\n", sampleNumber);
  printSystemStatus();
  Serial.println();
}

/**
 * 在 OLED 上显示传感器、告警和水泵状态。
 */
void displaySystemStatus() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x8_tf);

  oled.setCursor(0, 9);
  if (sensorDataAvailable && latestData.temperatureValid) {
    oled.printf("T:%.1fC", latestData.temperatureC);
  } else {
    oled.print("T:--");
  }

  oled.setCursor(0, 19);
  if (sensorDataAvailable && latestData.humidityValid) {
    oled.printf("H:%.1f%%", latestData.humidity);
  } else {
    oled.print("H:--");
  }

  oled.setCursor(0, 29);
  if (sensorDataAvailable && latestData.lightValid) {
    oled.printf("L:%.1flx", latestData.lightLux);
  } else {
    oled.print("L:--");
  }

  oled.setCursor(0, 39);
  if (sensorDataAvailable && latestData.soilValid) {
    oled.printf("S:%u", latestData.soilAdc);
  } else {
    oled.print("S:--");
  }

  oled.setCursor(0, 49);
  oled.print("Alert:");
  oled.print(alertStateText(currentAlertState));
  oled.setCursor(0, 60);
  oled.print("Pump:");
  oled.print(pumpStateText());
  oled.sendBuffer();
}

/**
 * 处理一个串口控制命令。
 *
 * @param command 从串口读取的命令字符。
 */
void handleCommand(char command) {
  if (command == '\r' || command == '\n') {
    return;
  }

  if (command == 'W' || command == 'w') {
    startPump();
    return;
  }

  if (command == 'S' || command == 's') {
    if (pumpRunning) {
      stopPump("manual stop");
    } else {
      Serial.println("Pump is already OFF.");
    }
    return;
  }

  if (command == 'H' || command == 'h') {
    printSystemStatus();
    printHelp();
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(command);
  printHelp();
}

/**
 * 初始化串口、I2C、传感器、OLED、ADC 和安全输出。
 */
void setup() {
  initializeOutputs();

  Serial.begin(SERIAL_BAUD_RATE);
  delay(STARTUP_DELAY_MS);
  Serial.println("Smart flower pot firmware starting.");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);

  oled.setI2CAddress(OLED_ADDRESS << 1);
  oled.begin();

  sht30Initialized = sht30.begin(SHT30_ADDRESS);
  bh1750Initialized = lightMeter.begin(
    BH1750::CONTINUOUS_HIGH_RES_MODE,
    BH1750_ADDRESS,
    &Wire
  );

  analogReadResolution(ADC_RESOLUTION_BITS);
  analogSetPinAttenuation(SOIL_SENSOR_PIN, ADC_11db);

  Serial.printf("SHT30: %s\n", sht30Initialized ? "OK" : "failed");
  Serial.printf("BH1750: %s\n", bh1750Initialized ? "OK" : "failed");
  Serial.println("Pump: OFF");
  printHelp();

  lastSensorReadAt = millis() - SENSOR_SAMPLE_INTERVAL_MS;
}

/**
 * 调度传感器采样、蜂鸣器、串口命令和水泵超时保护。
 */
void loop() {
  while (Serial.available() > 0) {
    handleCommand(static_cast<char>(Serial.read()));
  }

  if (pumpRunning && millis() - pumpStartedAt >= MAX_PUMP_RUNTIME_MS) {
    stopPump("timeout");
  }

  updateBuzzer(currentAlertState);

  const unsigned long now = millis();
  if (now - lastSensorReadAt < SENSOR_SAMPLE_INTERVAL_MS) {
    return;
  }

  lastSensorReadAt = now;
  latestData = readSensors();
  sensorDataAvailable = true;
  currentAlertState = updateAlertState(latestData);
  displaySystemStatus();
  printSampleData();
  sampleNumber++;
}
