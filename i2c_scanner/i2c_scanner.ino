#include <Wire.h>

constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SCAN_INTERVAL_MS = 3000;

/**
 * 扫描 I²C 有效地址并向串口输出应答结果。
 *
 * @return 本轮扫描发现的设备数量。
 */
uint8_t scanI2cBus() {
  uint8_t deviceCount = 0;

  Serial.println("I2C scan started.");
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    const uint8_t errorCode = Wire.endTransmission();

    if (errorCode == 0) {
      Serial.printf("Device found at 0x%02X\n", address);
      deviceCount++;
    } else if (errorCode != 2) {
      Serial.printf("I2C error at 0x%02X, code: %u\n", address, errorCode);
    }
  }

  if (deviceCount == 0) {
    Serial.println("No I2C devices found.");
    Serial.println("Check VDD, GND, SDA and SCL wiring.");
  }
  Serial.printf("Scan completed. %u device(s) found.\n\n", deviceCount);
  return deviceCount;
}

/**
 * 初始化 USB 串口和指定引脚的 I²C 总线。
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);
  Serial.println("ESP32-S3 I2C scanner ready.");
}

/**
 * 每三秒执行一次完整的 I²C 地址扫描。
 */
void loop() {
  scanI2cBus();
  delay(SCAN_INTERVAL_MS);
}
