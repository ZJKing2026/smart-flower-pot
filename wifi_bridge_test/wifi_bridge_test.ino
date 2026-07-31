#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_SHT31.h>
#include <BH1750.h>
#include <U8g2lib.h>
#include "wifi_secrets.h"

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
constexpr uint16_t ADC_MAX_VALUE = 4095;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SENSOR_SAMPLE_INTERVAL_MS = 2000;
constexpr unsigned long MAX_PUMP_RUNTIME_MS = 3000;
constexpr unsigned long BUZZER_INTERVAL_MS = 2000;
constexpr unsigned long BUZZER_DURATION_MS = 200;
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 10000;
constexpr unsigned long REPORT_INTERVAL_MS = 5000;
constexpr unsigned long COMMAND_POLL_INTERVAL_MS = 1000;
constexpr uint16_t HTTP_TIMEOUT_MS = 500;
constexpr float LOW_LIGHT_THRESHOLD_LX = 50.0F;
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

enum class AlertState : uint8_t { OK, LIGHT };

Adafruit_SHT31 sht30(&Wire);
BH1750 lightMeter;
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
WiFiClient wifiClient;

SensorData latestData = {};
AlertState currentAlertState = AlertState::OK;
bool sensorDataAvailable = false;
bool sht30Initialized = false;
bool bh1750Initialized = false;
bool pumpRunning = false;
bool buzzerPulseActive = false;
uint8_t lowLightFaultCount = 0;
uint8_t lowLightNormalCount = 0;
unsigned long pumpStartedAt = 0;
unsigned long lastSensorReadAt = 0;
unsigned long lastBuzzerPulseAt = 0;
unsigned long buzzerPulseStartedAt = 0;
unsigned long lastWifiRetryAt = 0;
unsigned long lastReportAt = 0;
unsigned long lastCommandPollAt = 0;

/** 获取告警文本。 */
const char *alertStateText(AlertState state) {
  return state == AlertState::LIGHT ? "LIGHT" : "OK";
}

/** 获取水泵状态文本。 */
const char *pumpStateText() {
  return pumpRunning ? "ON" : "OFF";
}

/** 初始化继电器和蜂鸣器至安全关闭状态。 */
void initializeOutputs() {
  pinMode(RELAY_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
}

/** 停止水泵。
 * @param reason 停止原因。
 */
void stopPump(const char *reason) {
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pumpRunning = false;
  Serial.printf("Pump: OFF, reason: %s\n", reason);
}

/** 启动受最长运行时间保护的水泵。 */
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

/** 读取温湿度、光照和土壤 ADC 数据。
 * @return 本轮传感器数据。
 */
SensorData readSensors() {
  SensorData data = {};
  data.temperatureC = sht30Initialized ? sht30.readTemperature() : NAN;
  data.humidity = sht30Initialized ? sht30.readHumidity() : NAN;
  data.lightLux = bh1750Initialized ? lightMeter.readLightLevel() : NAN;
  data.temperatureValid = !isnan(data.temperatureC) && data.temperatureC >= -40.0F && data.temperatureC <= 125.0F;
  data.humidityValid = !isnan(data.humidity) && data.humidity >= 0.0F && data.humidity <= 100.0F;
  data.lightValid = !isnan(data.lightLux) && data.lightLux >= 0.0F;

  uint32_t total = 0;
  data.soilValid = true;
  for (uint8_t index = 0; index < 20; index++) {
    const int value = analogRead(SOIL_SENSOR_PIN);
    if (value < 0 || value > ADC_MAX_VALUE) {
      data.soilValid = false;
    } else {
      total += static_cast<uint16_t>(value);
    }
    delay(5);
  }
  data.soilAdc = total / 20;
  return data;
}

/** 更新低光告警的消抖状态。
 * @param data 当前传感器数据。
 */
void updateAlertState(const SensorData &data) {
  if (!data.lightValid) {
    return;
  }
  if (data.lightLux < LOW_LIGHT_THRESHOLD_LX) {
    lowLightNormalCount = 0;
    if (lowLightFaultCount < ALERT_CONFIRM_COUNT) lowLightFaultCount++;
    if (lowLightFaultCount >= ALERT_CONFIRM_COUNT) currentAlertState = AlertState::LIGHT;
    return;
  }
  lowLightFaultCount = 0;
  if (lowLightNormalCount < ALERT_CONFIRM_COUNT) lowLightNormalCount++;
  if (lowLightNormalCount >= ALERT_CONFIRM_COUNT) currentAlertState = AlertState::OK;
}

/** 驱动低光告警蜂鸣器短响。 */
void updateBuzzer() {
  const unsigned long now = millis();
  if (currentAlertState == AlertState::OK) {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerPulseActive = false;
    return;
  }
  if (buzzerPulseActive && now - buzzerPulseStartedAt >= BUZZER_DURATION_MS) {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerPulseActive = false;
  }
  if (!buzzerPulseActive && now - lastBuzzerPulseAt >= BUZZER_INTERVAL_MS) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerPulseActive = true;
    buzzerPulseStartedAt = now;
    lastBuzzerPulseAt = now;
  }
}

/** 在 OLED 上显示当前状态。 */
void displaySystemStatus() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x8_tf);
  oled.setCursor(0, 9); oled.printf("T:%.1fC", latestData.temperatureC);
  oled.setCursor(0, 19); oled.printf("H:%.1f%%", latestData.humidity);
  oled.setCursor(0, 29); oled.printf("L:%.1flx", latestData.lightLux);
  oled.setCursor(0, 39); oled.printf("S:%u", latestData.soilAdc);
  oled.setCursor(0, 49); oled.printf("Alert:%s", alertStateText(currentAlertState));
  oled.setCursor(0, 60); oled.printf("Pump:%s", pumpStateText());
  oled.sendBuffer();
}

/** 输出串口状态。 */
void printSystemStatus() {
  Serial.printf("Temperature: %.1f C\nHumidity: %.1f %%\nLight: %.1f lx\nSoil ADC: %u\nAlert: %s\nPump: %s\n",
    latestData.temperatureC, latestData.humidity, latestData.lightLux, latestData.soilAdc,
    alertStateText(currentAlertState), pumpStateText());
}

/** 启动或重新发起热点连接。 */
void startWifiConnection() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("WiFi connecting to %s\n", WIFI_SSID);
}

/** 维护 Wi-Fi 连接，不阻塞本地控制。 */
void updateWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiRetryAt < WIFI_RETRY_INTERVAL_MS) return;
  lastWifiRetryAt = millis();
  WiFi.disconnect();
  startWifiConnection();
}

/** 生成当前状态的 JSON 上报内容。
 * @return JSON 字符串。
 */
String buildReportPayload() {
  char payload[220];
  snprintf(payload, sizeof(payload),
    "{\"temperature\":%.1f,\"humidity\":%.1f,\"light\":%.1f,\"soilAdc\":%u,\"alert\":\"%s\",\"pump\":\"%s\"}",
    latestData.temperatureC, latestData.humidity, latestData.lightLux, latestData.soilAdc,
    alertStateText(currentAlertState), pumpStateText());
  return String(payload);
}

/** 向电脑后端上报当前状态。 */
void reportDeviceStatus() {
  if (WiFi.status() != WL_CONNECTED || !sensorDataAvailable) return;
  HTTPClient http;
  const String url = String("http://") + BACKEND_HOST + ":" + BACKEND_PORT + "/api/device/report";
  http.begin(wifiClient, url);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  const int statusCode = http.POST(buildReportPayload());
  Serial.printf("Report HTTP: %d\n", statusCode);
  http.end();
}

/** 读取并执行一条网页控制命令。 */
void pollDeviceCommand() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  const String url = String("http://") + BACKEND_HOST + ":" + BACKEND_PORT + "/api/device/command";
  http.begin(wifiClient, url);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  const int statusCode = http.GET();
  const String response = statusCode == HTTP_CODE_OK ? http.getString() : "";
  http.end();
  if (statusCode != HTTP_CODE_OK) {
    Serial.printf("Command HTTP: %d\n", statusCode);
    return;
  }
  if (response.indexOf("\"action\": \"water\"") >= 0 || response.indexOf("\"action\":\"water\"") >= 0) {
    Serial.println("Web command: water");
    startPump();
  } else if (response.indexOf("\"action\": \"stop\"") >= 0 || response.indexOf("\"action\":\"stop\"") >= 0) {
    Serial.println("Web command: stop");
    stopPump("web command");
  }
}

/** 调度网络上报和命令轮询。 */
void updateNetworkTasks() {
  const unsigned long now = millis();
  if (now - lastReportAt >= REPORT_INTERVAL_MS) {
    lastReportAt = now;
    reportDeviceStatus();
  }
  if (now - lastCommandPollAt >= COMMAND_POLL_INTERVAL_MS) {
    lastCommandPollAt = now;
    pollDeviceCommand();
  }
}

/** 处理串口控制命令。
 * @param command 单字符命令。
 */
void handleCommand(char command) {
  if (command == 'W' || command == 'w') startPump();
  else if (command == 'S' || command == 's') stopPump("serial command");
  else if (command == 'H' || command == 'h') printSystemStatus();
}

/** 初始化硬件和网络。 */
void setup() {
  initializeOutputs();
  Serial.begin(SERIAL_BAUD_RATE);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  oled.setI2CAddress(OLED_ADDRESS << 1);
  oled.begin();
  sht30Initialized = sht30.begin(SHT30_ADDRESS);
  bh1750Initialized = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, BH1750_ADDRESS, &Wire);
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_SENSOR_PIN, ADC_11db);
  Serial.printf("SHT30: %s, BH1750: %s\n", sht30Initialized ? "OK" : "failed", bh1750Initialized ? "OK" : "failed");
  startWifiConnection();
  lastSensorReadAt = millis() - SENSOR_SAMPLE_INTERVAL_MS;
  lastReportAt = millis() - REPORT_INTERVAL_MS;
  lastCommandPollAt = millis() - COMMAND_POLL_INTERVAL_MS;
}

/** 调度本地传感器、执行器和网络任务。 */
void loop() {
  while (Serial.available() > 0) handleCommand(static_cast<char>(Serial.read()));
  if (pumpRunning && millis() - pumpStartedAt >= MAX_PUMP_RUNTIME_MS) stopPump("timeout");
  updateBuzzer();
  updateWifi();
  updateNetworkTasks();
  if (millis() - lastSensorReadAt < SENSOR_SAMPLE_INTERVAL_MS) return;
  lastSensorReadAt = millis();
  latestData = readSensors();
  sensorDataAvailable = true;
  updateAlertState(latestData);
  displaySystemStatus();
  printSystemStatus();
}
