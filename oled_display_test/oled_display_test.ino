#include <Wire.h>
#include <U8g2lib.h>

constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long REFRESH_INTERVAL_MS = 1000;

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(
  U8G2_R0,
  U8X8_PIN_NONE
);

/**
 * 绘制 OLED 固定测试信息和当前刷新计数。
 *
 * @param refreshCount 当前画面刷新次数。
 */
void renderTestScreen(unsigned long refreshCount) {
  oled.clearBuffer();
  oled.drawFrame(0, 0, 128, 64);
  oled.setFont(u8g2_font_6x10_tf);
  oled.setCursor(4, 12);
  oled.print("Smart Flower Pot");
  oled.setCursor(4, 27);
  oled.print("OLED Test");
  oled.setCursor(4, 42);
  oled.print("Address: 0x3C");
  oled.setCursor(4, 57);
  oled.print("Status: OK #");
  oled.print(refreshCount);
  oled.sendBuffer();
}

/**
 * 初始化 USB 串口、I²C 总线和 SH1106 OLED。
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);
  oled.setI2CAddress(OLED_ADDRESS << 1);
  oled.begin();
  Serial.println("SH1106 OLED display test started.");
}

/**
 * 每秒刷新一次 OLED 测试画面和串口计数。
 */
void loop() {
  static unsigned long refreshCount = 1;

  renderTestScreen(refreshCount);
  Serial.printf("OLED refresh: %lu\n", refreshCount);
  refreshCount++;
  delay(REFRESH_INTERVAL_MS);
}
