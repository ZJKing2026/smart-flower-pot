#include <Arduino.h>

constexpr uint8_t RELAY_PIN = 10;
constexpr uint8_t RELAY_ON = LOW;
constexpr uint8_t RELAY_OFF = HIGH;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SERIAL_STARTUP_DELAY_MS = 1000;
constexpr unsigned long MAX_PUMP_RUNTIME_MS = 3000;

bool pumpRunning = false;
unsigned long pumpStartedAt = 0;

/**
 * 输出串口控制命令说明。
 */
void printHelp() {
  Serial.println("Commands: W = water for up to 3 seconds, S = stop pump.");
}

/**
 * 将继电器切换为关闭状态，并输出停止原因。
 *
 * @param reason 停止水泵的原因。
 */
void stopPump(const char *reason) {
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pumpRunning = false;
  Serial.print("Pump: OFF, reason: ");
  Serial.println(reason);
}

/**
 * 启动一次手动浇水，并记录启动时间。
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
 * 处理单个串口控制字符。
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

  Serial.print("Unknown command: ");
  Serial.println(command);
  printHelp();
}

/**
 * 初始化继电器为关闭状态，并启动串口控制台。
 */
void setup() {
  pinMode(RELAY_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  Serial.begin(SERIAL_BAUD_RATE);
  delay(SERIAL_STARTUP_DELAY_MS);

  Serial.println("Manual watering test started.");
  Serial.println("Pump: OFF");
  printHelp();
}

/**
 * 处理串口命令并执行水泵超时保护。
 */
void loop() {
  while (Serial.available() > 0) {
    const char command = static_cast<char>(Serial.read());
    handleCommand(command);
  }

  if (pumpRunning && millis() - pumpStartedAt >= MAX_PUMP_RUNTIME_MS) {
    stopPump("timeout");
  }
}
