# 多传感器与 OLED 第一阶段集成实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建独立集成草图，每2秒读取温度、空气湿度、光照和土壤ADC，并同步显示到串口与OLED。

**Architecture:** SHT30、BH1750和SH1106共用GPIO8/9的I²C总线，土壤传感器使用GPIO1 ADC。每个传感器独立记录可用状态，单个设备失败不会阻塞其他数据采集和显示。

**Tech Stack:** Arduino/C++、ESP32 Arduino Core 3.3.7、Adafruit SHT31、BH1750、U8g2、Arduino CLI。

---

## 文件结构

- 创建：`sensor_oled_integration_test/sensor_oled_integration_test.ino`，负责初始化、采集、校验、串口输出和OLED刷新。
- 修改：`docs/project-progress.md`，仅在完成2分钟实机验证后记录结果。

### Task 1: 创建集成测试草图

**Files:**
- Create: `sensor_oled_integration_test/sensor_oled_integration_test.ino`

- [ ] **Step 1: 确认目标草图尚不存在**

Run:

```powershell
Test-Path -LiteralPath 'sensor_oled_integration_test\sensor_oled_integration_test.ino'
```

Expected: `False`

- [ ] **Step 2: 创建完整草图**

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <BH1750.h>
#include <U8g2lib.h>

constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;
constexpr uint8_t SOIL_SENSOR_PIN = 1;
constexpr uint8_t SHT30_ADDRESS = 0x44;
constexpr uint8_t BH1750_ADDRESS = 0x23;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint8_t ADC_RESOLUTION_BITS = 12;
constexpr uint16_t ADC_MAX_VALUE = 4095;
constexpr uint8_t SOIL_SAMPLE_COUNT = 20;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SERIAL_STARTUP_DELAY_MS = 1000;
constexpr unsigned long SOIL_SAMPLE_DELAY_MS = 5;
constexpr unsigned long DISPLAY_INTERVAL_MS = 2000;
constexpr unsigned long START_SCREEN_DURATION_MS = 1000;
constexpr float MIN_TEMPERATURE_C = -40.0F;
constexpr float MAX_TEMPERATURE_C = 125.0F;
constexpr float MIN_HUMIDITY_PERCENT = 0.0F;
constexpr float MAX_HUMIDITY_PERCENT = 100.0F;

struct SensorData {
  float temperatureC;
  float humidityPercent;
  float lightLevelLux;
  uint16_t soilAdc;
  bool temperatureHumidityValid;
  bool lightValid;
};

Adafruit_SHT31 sht30(&Wire);
BH1750 lightMeter;
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);

bool sht30Ready = false;
bool bh1750Ready = false;

/**
 * 连续采集土壤传感器ADC并返回平均值。
 *
 * @return 20次ADC采样的平均值。
 */
uint16_t sampleSoilAdc() {
  uint32_t total = 0;
  for (uint8_t index = 0; index < SOIL_SAMPLE_COUNT; index++) {
    total += analogRead(SOIL_SENSOR_PIN);
    delay(SOIL_SAMPLE_DELAY_MS);
  }
  return total / SOIL_SAMPLE_COUNT;
}

/**
 * 读取并校验本轮全部传感器数据。
 *
 * @return 本轮温湿度、光照、土壤ADC及有效状态。
 */
SensorData readSensorData() {
  SensorData data = {
    NAN,
    NAN,
    NAN,
    sampleSoilAdc(),
    false,
    false
  };

  if (sht30Ready) {
    data.temperatureC = sht30.readTemperature();
    data.humidityPercent = sht30.readHumidity();
    data.temperatureHumidityValid =
      !isnan(data.temperatureC) &&
      !isnan(data.humidityPercent) &&
      data.temperatureC >= MIN_TEMPERATURE_C &&
      data.temperatureC <= MAX_TEMPERATURE_C &&
      data.humidityPercent >= MIN_HUMIDITY_PERCENT &&
      data.humidityPercent <= MAX_HUMIDITY_PERCENT;
  }

  if (bh1750Ready) {
    data.lightLevelLux = lightMeter.readLightLevel();
    data.lightValid =
      !isnan(data.lightLevelLux) && data.lightLevelLux >= 0.0F;
  }

  return data;
}

/**
 * 将本轮传感器数据输出到串口。
 *
 * @param sampleNumber 当前采样编号。
 * @param data 当前传感器数据。
 */
void printSensorData(unsigned long sampleNumber, const SensorData &data) {
  Serial.printf("Sample #%lu\n", sampleNumber);
  if (data.temperatureHumidityValid) {
    Serial.printf("Temperature: %.1f C\n", data.temperatureC);
    Serial.printf("Humidity: %.1f %%\n", data.humidityPercent);
  } else {
    Serial.println("Temperature: invalid");
    Serial.println("Humidity: invalid");
  }

  if (data.lightValid) {
    Serial.printf("Light: %.1f lx\n", data.lightLevelLux);
  } else {
    Serial.println("Light: invalid");
  }
  Serial.printf("Soil ADC: %u\n\n", data.soilAdc);
}

/**
 * 将本轮传感器数据刷新到OLED。
 *
 * @param data 当前传感器数据。
 */
void drawSensorData(const SensorData &data) {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tf);

  display.setCursor(0, 11);
  display.print("Temp:  ");
  if (data.temperatureHumidityValid) {
    display.print(data.temperatureC, 1);
    display.print(" C");
  } else {
    display.print("-- C");
  }

  display.setCursor(0, 26);
  display.print("Hum:   ");
  if (data.temperatureHumidityValid) {
    display.print(data.humidityPercent, 1);
    display.print(" %");
  } else {
    display.print("-- %");
  }

  display.setCursor(0, 41);
  display.print("Light: ");
  if (data.lightValid) {
    display.print(data.lightLevelLux, 0);
    display.print(" lx");
  } else {
    display.print("-- lx");
  }

  display.setCursor(0, 56);
  display.print("Soil:  ");
  display.print(data.soilAdc);
  display.print(" ADC");
  display.sendBuffer();
}

/**
 * 显示集成测试启动页面。
 */
void drawStartScreen() {
  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);
  display.setCursor(4, 27);
  display.print("Smart Flower Pot");
  display.setCursor(16, 45);
  display.print("Sensor Test");
  display.sendBuffer();
}

/**
 * 初始化串口、I2C、OLED、传感器和ADC。
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(SERIAL_STARTUP_DELAY_MS);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);

  display.setI2CAddress(OLED_ADDRESS << 1);
  display.begin();
  drawStartScreen();

  sht30Ready = sht30.begin(SHT30_ADDRESS);
  bh1750Ready = lightMeter.begin(
    BH1750::CONTINUOUS_HIGH_RES_MODE,
    BH1750_ADDRESS,
    &Wire
  );

  analogReadResolution(ADC_RESOLUTION_BITS);
  analogSetPinAttenuation(SOIL_SENSOR_PIN, ADC_11db);

  Serial.printf("OLED: ready at 0x%02X\n", OLED_ADDRESS);
  Serial.printf("SHT30: %s\n", sht30Ready ? "ready" : "not found");
  Serial.printf("BH1750: %s\n", bh1750Ready ? "ready" : "not found");
  Serial.println("Sensor and OLED integration test started.\n");
  delay(START_SCREEN_DURATION_MS);
}

/**
 * 每两秒采集并显示一次全部传感器数据。
 */
void loop() {
  static unsigned long sampleNumber = 1;
  const SensorData data = readSensorData();
  printSensorData(sampleNumber, data);
  drawSensorData(data);
  sampleNumber++;
  delay(DISPLAY_INTERVAL_MS);
}
```

- [ ] **Step 3: 静态检查功能边界和引脚**

Run:

```powershell
Select-String -LiteralPath 'sensor_oled_integration_test\sensor_oled_integration_test.ino' -Pattern 'I2C_SDA_PIN = 8|I2C_SCL_PIN = 9|SOIL_SENSOR_PIN = 1|SHT30_ADDRESS = 0x44|BH1750_ADDRESS = 0x23|OLED_ADDRESS = 0x3C|SOIL_SAMPLE_COUNT = 20|DISPLAY_INTERVAL_MS = 2000'
```

Expected: 输出全部引脚、地址、采样数和刷新间隔配置。

- [ ] **Step 4: 检查未引入执行器逻辑**

Run:

```powershell
Select-String -LiteralPath 'sensor_oled_integration_test\sensor_oled_integration_test.ino' -Pattern 'RELAY|BUZZER|PUMP|WiFi|MQTT'
```

Expected: 无输出。

### Task 2: 编译验证

**Files:**
- Test: `sensor_oled_integration_test/sensor_oled_integration_test.ino`

- [ ] **Step 1: 使用项目内库编译**

Run:

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile `
  --fqbn esp32:esp32:esp32s3 `
  --libraries 'D:\AAAAA\smart_flower_pot\libraries' `
  --build-path 'D:\AAAAA\smart_flower_pot\.build\sensor_oled_integration_test' `
  'D:\AAAAA\smart_flower_pot\sensor_oled_integration_test'
```

Expected: 退出码为0，输出程序空间及动态内存占用。

### Task 3: 烧录和实机验证

**Files:**
- Test: `sensor_oled_integration_test/sensor_oled_integration_test.ino`
- Modify after pass: `docs/project-progress.md`

- [ ] **Step 1: 烧录前断开执行器**

断开蜂鸣器 `I/O`、继电器 `IN` 和水泵负载端，仅保留OLED、SHT30、BH1750及土壤传感器。

- [ ] **Step 2: 烧录并检查初始化输出**

Expected:

```text
OLED: ready at 0x3C
SHT30: ready
BH1750: ready
Sensor and OLED integration test started.
```

- [ ] **Step 3: 检查OLED和串口同步刷新**

确认温度、湿度、光照和土壤ADC每2秒刷新，OLED无截断或重叠，串口数据与OLED一致。

- [ ] **Step 4: 进行响应测试**

```text
遮挡BH1750：光照值明显下降，移开后恢复
靠近或轻触SHT30：温湿度产生合理变化并逐渐恢复
触碰土壤探头：ADC按独立测试规律变化
```

- [ ] **Step 5: 连续运行2分钟**

Expected: 无死机、OLED停刷、无效数据持续出现或I²C设备丢失。

- [ ] **Step 6: 记录验证结果**

仅在上述检查全部通过后，将编译占用、四项读数范围和2分钟稳定性结果追加到 `docs/project-progress.md`。

## 版本控制说明

当前工作区不是 Git 仓库，因此不执行提交操作，也不擅自初始化 Git。

