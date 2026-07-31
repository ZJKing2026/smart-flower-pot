#include <Arduino.h>

constexpr uint8_t RELAY_PIN = 10;
constexpr uint8_t RELAY_ON = LOW;
constexpr uint8_t RELAY_OFF = HIGH;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SERIAL_STARTUP_DELAY_MS = 1000;
constexpr unsigned long TEST_START_DELAY_MS = 3000;
constexpr unsigned long MAX_PUMP_RUNTIME_MS = 2000;
constexpr unsigned long SAFE_IDLE_DELAY_MS = 1000;

/**
 * @brief 设置水泵继电器状态并输出当前状态。
 * @param isOn true 表示启动水泵，false 表示关闭水泵。
 */
void setPumpState(bool isOn) {
  digitalWrite(RELAY_PIN, isOn ? RELAY_ON : RELAY_OFF);
  Serial.println(isOn ? "Pump: ON" : "Pump: OFF");
}

/**
 * @brief 初始化继电器与串口，并执行一次限时水泵测试。
 */
void setup() {
  pinMode(RELAY_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  Serial.begin(SERIAL_BAUD_RATE);
  delay(SERIAL_STARTUP_DELAY_MS);

  Serial.println("Pump relay test started.");
  setPumpState(false);
  Serial.println("Pump starts in 3 seconds.");
  delay(TEST_START_DELAY_MS);

  setPumpState(true);
  delay(MAX_PUMP_RUNTIME_MS);
  setPumpState(false);

  Serial.println("Pump relay test completed. Pump remains OFF.");
}

/**
 * @brief 持续保持继电器关闭，防止水泵再次启动。
 */
void loop() {
  digitalWrite(RELAY_PIN, RELAY_OFF);
  delay(SAFE_IDLE_DELAY_MS);
}
