void setup() {
  // 初始化串口并输出板卡启动提示。
  constexpr unsigned long SERIAL_BAUD_RATE = 115200;

  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  Serial.println("ESP32-S3 serial check started.");
}

/**
 * 每秒输出一次递增的串口心跳序号。
 */
void loop() {
  constexpr unsigned long HEARTBEAT_INTERVAL_MS = 1000;
  static unsigned long heartbeatCount = 1;

  Serial.print("Heartbeat: ");
  Serial.println(heartbeatCount);
  heartbeatCount++;
  delay(HEARTBEAT_INTERVAL_MS);
}
