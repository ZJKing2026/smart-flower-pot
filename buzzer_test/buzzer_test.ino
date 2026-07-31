#include <Arduino.h>

constexpr uint8_t BUZZER_PIN = 11;
constexpr uint8_t BUZZER_ON_LEVEL = LOW;
constexpr uint8_t BUZZER_OFF_LEVEL = HIGH;
constexpr uint8_t BEEP_COUNT = 3;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SERIAL_STARTUP_DELAY_MS = 1000;
constexpr unsigned long TEST_START_DELAY_MS = 3000;
constexpr unsigned long BEEP_DURATION_MS = 300;
constexpr unsigned long BEEP_INTERVAL_MS = 300;
constexpr unsigned long SAFETY_IDLE_DELAY_MS = 1000;

/**
 * @brief 设置蜂鸣器启停状态并输出当前状态。
 *
 * @param enabled true 表示鸣叫，false 表示静音。
 */
void setBuzzerState(bool enabled) {
  digitalWrite(
    BUZZER_PIN,
    enabled ? BUZZER_ON_LEVEL : BUZZER_OFF_LEVEL
  );
  Serial.println(enabled ? "Buzzer: ON" : "Buzzer: OFF");
}

/**
 * @brief 安全初始化蜂鸣器并执行三次鸣叫测试。
 */
void setup() {
  pinMode(BUZZER_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, BUZZER_OFF_LEVEL);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, BUZZER_OFF_LEVEL);

  Serial.begin(SERIAL_BAUD_RATE);
  delay(SERIAL_STARTUP_DELAY_MS);

  Serial.println("Buzzer test started.");
  setBuzzerState(false);
  Serial.println("Test starts in 3 seconds.");
  delay(TEST_START_DELAY_MS);

  for (uint8_t beepNumber = 1; beepNumber <= BEEP_COUNT; beepNumber++) {
    Serial.print("Beep ");
    Serial.print(beepNumber);
    Serial.print("/");
    Serial.println(BEEP_COUNT);

    setBuzzerState(true);
    delay(BEEP_DURATION_MS);
    setBuzzerState(false);
    delay(BEEP_INTERVAL_MS);
  }

  setBuzzerState(false);
  Serial.println("Buzzer test completed.");
  Serial.println("Buzzer remains OFF.");
}

/**
 * @brief 持续保持蜂鸣器关闭，防止意外鸣叫。
 */
void loop() {
  digitalWrite(BUZZER_PIN, BUZZER_OFF_LEVEL);
  delay(SAFETY_IDLE_DELAY_MS);
}
