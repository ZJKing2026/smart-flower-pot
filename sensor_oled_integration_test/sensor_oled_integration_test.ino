#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <BH1750.h>
#include <U8g2lib.h>

constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;
constexpr uint8_t SOIL_SENSOR_PIN = 1;
constexpr uint8_t BUZZER_PIN = 11;
constexpr uint8_t SHT30_ADDRESS = 0x44;
constexpr uint8_t BH1750_ADDRESS = 0x23;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr uint8_t ADC_RESOLUTION_BITS = 12;
constexpr uint16_t ADC_MAX_VALUE = 4095;
constexpr uint8_t SOIL_SAMPLE_COUNT = 20;
constexpr unsigned long SOIL_SAMPLE_DELAY = 5;
constexpr unsigned long SERIAL_STARTUP_DELAY = 1000;
constexpr unsigned long START_SCREEN_DELAY = 1000;
constexpr unsigned long SENSOR_SAMPLE_INTERVAL = 2000;
constexpr unsigned long BUZZER_INTERVAL = 2000;
constexpr unsigned long BUZZER_DURATION = 200;
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

bool sht30Initialized = false;
bool bh1750Initialized = false;
bool lowLightAlertActive = false;
bool drySoilAlertActive = false;
bool buzzerPulseActive = false;
uint8_t lowLightFaultCount = 0;
uint8_t lowLightNormalCount = 0;
uint8_t drySoilFaultCount = 0;
uint8_t drySoilNormalCount = 0;
unsigned long lastSensorReadAt = 0;
unsigned long lastBuzzerPulseAt = 0;
unsigned long buzzerPulseStartedAt = 0;

/**
 * 在 OLED 上显示传感器集成程序的启动画面。
 */
void showStartScreen() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.setCursor(13, 27);
  oled.print("Sensor Integration");
  oled.setCursor(37, 43);
  oled.print("Starting...");
  oled.sendBuffer();
}

/**
 * 读取并校验 SHT30 温湿度数据。
 *
 * @param data 保存本轮温度、湿度及其有效状态的数据结构。
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
 * @param data 保存本轮光照值及其有效状态的数据结构。
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
 * 连续采集土壤传感器 ADC，并计算本轮平均值。
 *
 * @param data 保存本轮土壤 ADC 平均值及其有效状态的数据结构。
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
    delay(SOIL_SAMPLE_DELAY);
  }

  data.soilValid = samplesValid;
  if (samplesValid) {
    data.soilAdc = total / SOIL_SAMPLE_COUNT;
  }
}

/**
 * 采集一轮全部传感器数据。
 *
 * @return 包含四项读数及各自有效状态的数据结构。
 */
SensorData readSensors() {
  SensorData data = {};

  readSht30(data);
  readBh1750(data);
  readSoilAdc(data);
  return data;
}

/**
 * 更新单项告警的防抖状态。
 *
 * @param measurementValid 本轮测量是否有效。
 * @param abnormal 本轮是否满足异常条件。
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
 * 根据当前传感器读数更新低光照和土壤偏干告警状态。
 *
 * @param data 当前一轮传感器读数和有效状态。
 * @return 更新后的综合告警状态。
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
 * 获取综合告警状态的显示文本。
 *
 * @param state 当前综合告警状态。
 * @return 用于串口和 OLED 显示的状态文本。
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
 * 初始化低电平触发蜂鸣器并保持静音。
 */
void initializeBuzzer() {
  pinMode(BUZZER_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, HIGH);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
}

/**
 * 依据告警状态非阻塞控制蜂鸣器短鸣。
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
    if (now - buzzerPulseStartedAt >= BUZZER_DURATION) {
      digitalWrite(BUZZER_PIN, HIGH);
      buzzerPulseActive = false;
    }
    return;
  }

  if (now - lastBuzzerPulseAt >= BUZZER_INTERVAL) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerPulseActive = true;
    buzzerPulseStartedAt = now;
    lastBuzzerPulseAt = now;
  }
}

/**
 * 将一轮传感器数据和告警状态输出到串口。
 *
 * @param sampleNumber 当前采样编号。
 * @param data 当前一轮传感器读数和有效状态。
 * @param state 当前综合告警状态。
 */
void printSensorData(
  unsigned long sampleNumber,
  const SensorData &data,
  AlertState state
) {
  Serial.printf("Sample #%lu\n", sampleNumber);

  Serial.print("Temperature: ");
  if (data.temperatureValid) {
    Serial.printf("%.1f C\n", data.temperatureC);
  } else {
    Serial.println("invalid");
  }

  Serial.print("Humidity: ");
  if (data.humidityValid) {
    Serial.printf("%.1f %%\n", data.humidity);
  } else {
    Serial.println("invalid");
  }

  Serial.print("Light: ");
  if (data.lightValid) {
    Serial.printf("%.1f lx\n", data.lightLux);
  } else {
    Serial.println("invalid");
  }

  Serial.print("Soil ADC: ");
  if (data.soilValid) {
    Serial.println(data.soilAdc);
  } else {
    Serial.println("invalid");
  }

  Serial.print("Alert: ");
  Serial.println(alertStateText(state));
  Serial.println();
}

/**
 * 在 OLED 上显示一轮传感器数据和当前告警状态。
 *
 * @param data 当前一轮传感器读数和有效状态。
 * @param state 当前综合告警状态。
 */
void displaySensorData(const SensorData &data, AlertState state) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);

  oled.setCursor(0, 11);
  oled.print("T:");
  if (data.temperatureValid) {
    oled.print(data.temperatureC, 1);
    oled.print("C");
  } else {
    oled.print("--");
  }

  oled.setCursor(0, 23);
  oled.print("H:");
  if (data.humidityValid) {
    oled.print(data.humidity, 1);
    oled.print("%");
  } else {
    oled.print("--");
  }

  oled.setCursor(0, 35);
  oled.print("L:");
  if (data.lightValid) {
    oled.print(data.lightLux, 1);
    oled.print("lx");
  } else {
    oled.print("--");
  }

  oled.setCursor(0, 47);
  oled.print("S:");
  if (data.soilValid) {
    oled.print(data.soilAdc);
  } else {
    oled.print("--");
  }

  oled.setCursor(0, 60);
  oled.print("Alert:");
  oled.print(alertStateText(state));
  oled.sendBuffer();
}

/**
 * 依次初始化串口、I2C、OLED、传感器、ADC 和蜂鸣器。
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(SERIAL_STARTUP_DELAY);
  Serial.println("Sensor and OLED integration test starting.");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);
  Serial.println("I2C initialized: SDA GPIO8, SCL GPIO9, 100 kHz.");

  oled.setI2CAddress(OLED_ADDRESS << 1);
  oled.begin();
  showStartScreen();
  Serial.println("SH1106 OLED initialized at address 0x3C.");
  delay(START_SCREEN_DELAY);

  sht30Initialized = sht30.begin(SHT30_ADDRESS);
  Serial.printf(
    "SHT30 initialization at address 0x44: %s\n",
    sht30Initialized ? "OK" : "failed"
  );

  bh1750Initialized = lightMeter.begin(
    BH1750::CONTINUOUS_HIGH_RES_MODE,
    BH1750_ADDRESS,
    &Wire
  );
  Serial.printf(
    "BH1750 initialization at address 0x23: %s\n",
    bh1750Initialized ? "OK" : "failed"
  );

  analogReadResolution(ADC_RESOLUTION_BITS);
  analogSetPinAttenuation(SOIL_SENSOR_PIN, ADC_11db);
  Serial.println("Soil ADC initialized: GPIO1, 12-bit, ADC_11db.");

  initializeBuzzer();
  Serial.println("Buzzer initialized: GPIO11, active LOW.");
  Serial.println();

  lastSensorReadAt = millis() - SENSOR_SAMPLE_INTERVAL;
}

/**
 * 每两秒采集并显示传感器数据，同时持续更新蜂鸣器短鸣状态。
 */
void loop() {
  static unsigned long sampleNumber = 1;
  static AlertState alertState = AlertState::OK;

  updateBuzzer(alertState);

  const unsigned long now = millis();
  if (now - lastSensorReadAt < SENSOR_SAMPLE_INTERVAL) {
    return;
  }

  lastSensorReadAt = now;
  const SensorData data = readSensors();
  alertState = updateAlertState(data);

  printSensorData(sampleNumber, data, alertState);
  displaySensorData(data, alertState);
  sampleNumber++;
  updateBuzzer(alertState);
}
