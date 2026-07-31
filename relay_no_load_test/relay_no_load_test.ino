constexpr uint8_t RELAY_PIN = 10;
constexpr uint8_t RELAY_ON_LEVEL = LOW;
constexpr uint8_t RELAY_OFF_LEVEL = HIGH;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SERIAL_STARTUP_DELAY_MS = 1000;
constexpr unsigned long TEST_START_DELAY_MS = 3000;
constexpr unsigned long RELAY_ON_DURATION_MS = 5000;
constexpr unsigned long RELAY_OFF_DURATION_MS = 5000;

/**
 * 设置继电器状态并输出当前状态。
 *
 * @param enabled true表示吸合，false表示释放。
 */
void setRelayState(bool enabled) {
  const uint8_t outputLevel = enabled ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL;
  digitalWrite(RELAY_PIN, outputLevel);
  Serial.printf("Relay: %s\n", enabled ? "ON" : "OFF");
}

/**
 * 初始化串口和继电器，使继电器在启动阶段保持释放。
 */
void setup() {
  digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);
  pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);

  Serial.begin(SERIAL_BAUD_RATE);
  delay(SERIAL_STARTUP_DELAY_MS);
  Serial.println("Relay continuous no-load test started.");

  setRelayState(false);
  Serial.printf(
    "Test begins in %lu seconds.\n",
    TEST_START_DELAY_MS / 1000
  );
  delay(TEST_START_DELAY_MS);
}

/**
 * 以五秒间隔持续切换继电器，便于观察和测量控制电平。
 */
void loop() {
  setRelayState(true);
  delay(RELAY_ON_DURATION_MS);
  setRelayState(false);
  delay(RELAY_OFF_DURATION_MS);
}
