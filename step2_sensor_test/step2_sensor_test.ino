/*
 * Step 2 - 传感器裸机测试 (含 OLED 显示)
 * 目的：验证 SHT30、BH1750、土壤湿度传感器是否正常读数
 *       并在 SH1106 OLED 上实时显示
 * 接线：SHT30/BH1750/SH1106 并联 I2C，土壤湿度 OUT->GPIO1
 * 库：Adafruit SHT31、BH1750、U8g2
 */

#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_SHT31.h>
#include <BH1750.h>

// ==================== 引脚定义 ====================
#define SOIL_MOISTURE_PIN  1     // ADC1_CH0
#define RELAY_PUMP_PIN     10
#define BUZZER_PIN         12    // 低电平触发
#define LED_PIN            13

// ==================== I2C 地址 ====================
#define SHT30_ADDR   0x44
#define BH1750_ADDR  0x23

// ==================== 对象声明 ====================
Adafruit_SHT31 sht30 = Adafruit_SHT31(&Wire);
BH1750 bh1750;

// SH1106 1.3寸 128x64 OLED，硬件 I2C，无复位引脚
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ==================== 土壤湿度校准值 ====================
// 在空气中干燥时的 ADC 值（约 3000），完全浸水时（约 1200）
const int AIR_VALUE    = 3000;
const int WATER_VALUE  = 1200;

// ==================== 传感器状态 ====================
bool sht30_ok  = false;
bool bh1750_ok = false;

// ==================== I2C 重定向工具 ====================
void beginI2C() {
  Wire.begin(8, 9);       // SDA=GPIO8, SCL=GPIO9
  Wire.setClock(100000);  // 100kHz 标准模式
}

// ==================== 蜂鸣器自检 ====================
void buzzerBeep(int durationMs) {
  digitalWrite(BUZZER_PIN, LOW);   // 低电平触发
  delay(durationMs);
  digitalWrite(BUZZER_PIN, HIGH);  // 关闭
}

// ==================== OLED 初始化 ====================
bool initOLED() {
  u8g2.begin();
  u8g2.enableUTF8Print();  // 启用中文（如果需要）
  return true;
}

// ==================== OLED 显示数据 ====================
void displayData(float temp, float hum, float lux, int soilRaw, int soilPct) {
  u8g2.firstPage();
  do {
    // ---- 标题栏 ----
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(0, 9);
    u8g2.print("Smart Flower Pot");

    // 分隔线
    u8g2.drawHLine(0, 12, 128);

    // ---- 温度 ----
    u8g2.setCursor(0, 24);
    u8g2.print("Temp: ");
    if (!isnan(temp)) {
      u8g2.print(temp, 1);
      u8g2.print(" C");
    } else {
      u8g2.print("-- C");
    }

    // ---- 湿度 ----
    u8g2.setCursor(0, 36);
    u8g2.print("Hum:  ");
    if (!isnan(hum)) {
      u8g2.print(hum, 1);
      u8g2.print(" %");
    } else {
      u8g2.print("-- %");
    }

    // ---- 光照 ----
    u8g2.setCursor(0, 48);
    u8g2.print("Lux:  ");
    if (lux >= 0) {
      u8g2.print(lux, 0);
      u8g2.print(" lx");
    } else {
      u8g2.print("-- lx");
    }

    // ---- 土壤湿度 ----
    u8g2.setCursor(0, 60);
    u8g2.print("Soil: ");
    u8g2.print(soilRaw);
    u8g2.print(" (");
    u8g2.print(soilPct);
    u8g2.print("%)");

  } while (u8g2.nextPage());
}

// ==================== setup ====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("==================================");
  Serial.println(" Step 2 - 传感器裸机测试 (含 OLED)");
  Serial.println(" ESP32-S3 智能花盆");
  Serial.println("==================================");
  Serial.println();

  // 初始化 I2C
  beginI2C();

  // 引脚模式
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PUMP_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH);   // 高电平=关闭（低电平触发）
  digitalWrite(RELAY_PUMP_PIN, LOW);

  // ---- 开机蜂鸣器自检 ----
  Serial.print("[BUZZER] 自检...");
  buzzerBeep(150);
  delay(100);
  buzzerBeep(100);
  Serial.println(" 完成 ✅");

  // ---- LED 闪烁 ----
  digitalWrite(LED_PIN, HIGH);
  delay(200);
  digitalWrite(LED_PIN, LOW);

  // ---- OLED 初始化 ----
  Serial.print("[OLED]  正在初始化...");
  if (initOLED()) {
    Serial.println(" 完成 ✅");
  }

  // ---- SHT30 ----
  Serial.print("[SHT30] 正在检测...");
  if (sht30.begin(SHT30_ADDR)) {
    sht30_ok = true;
    Serial.println(" 已找到 ✅");
  } else {
    Serial.println(" 未找到 ❌");
  }

  // ---- BH1750 ----
  Serial.print("[BH1750] 正在检测...");
  if (bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, BH1750_ADDR, &Wire)) {
    bh1750_ok = true;
    Serial.println(" 已找到 ✅");
  } else {
    Serial.println(" 未找到 ❌");
  }

  // ---- I2C 扫描 ----
  Serial.println("\n[I2C] 扫描总线...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  设备 @ 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
    }
  }
  Serial.println("[I2C] 扫描完成");

  // ---- 屏幕显示欢迎 ----
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_10x20_tf);
    u8g2.setCursor(8, 30);
    u8g2.print("Sensor");
    u8g2.setCursor(8, 52);
    u8g2.print("Test OK!");
  } while (u8g2.nextPage());
  delay(1500);

  Serial.println("\n每2秒刷新数据:");
  Serial.println("温度   | 湿度   | 光照     | 土壤ADC(百分比)");
  Serial.println("--------------------------------------------");
}

// ==================== loop ====================
void loop() {
  // ---- 读取 SHT30 ----
  float temp = NAN, hum = NAN;
  if (sht30_ok) {
    temp = sht30.readTemperature();
    hum  = sht30.readHumidity();
  }

  // ---- 读取 BH1750 ----
  float lux = -1;
  if (bh1750_ok) lux = bh1750.readLightLevel();

  // ---- 读取土壤湿度 ----
  int soilRaw = analogRead(SOIL_MOISTURE_PIN);
  int soilPercent = constrain(
    map(soilRaw, AIR_VALUE, WATER_VALUE, 0, 100),
    0, 100
  );

  // ---- 串口输出 ----
  if (!isnan(temp)) { Serial.print(temp, 1); Serial.print("C   "); }
  else              { Serial.print("--C   "); }

  if (!isnan(hum))  { Serial.print(hum, 1); Serial.print("%   "); }
  else              { Serial.print("--%   "); }

  if (lux >= 0)     { Serial.print(lux, 0); Serial.print(" lx   "); }
  else              { Serial.print("--- lx   "); }

  Serial.print(soilRaw);
  Serial.print(" (");
  Serial.print(soilPercent);
  Serial.println("%)");

  // ---- OLED 显示 ----
  displayData(temp, hum, lux, soilRaw, soilPercent);

  // ---- LED 心跳 ----
  digitalWrite(LED_PIN, !digitalRead(LED_PIN));

  delay(2000);
}
