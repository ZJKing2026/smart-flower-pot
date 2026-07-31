# SHT30 Sensor Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建一个独立草图，每两秒读取并校验 SHT30 的温度和相对湿度，然后通过串口输出。

**Architecture:** 使用 Adafruit SHT31 Library 和共享的 Arduino `Wire` 对象。`setup()` 初始化 GPIO8/GPIO9 I²C 总线及地址 `0x44`；`readAndPrintMeasurement()` 负责读取、范围校验和串口输出；主循环持续采样，但初始化失败时不读取无效数据。

**Tech Stack:** Arduino C++、ESP32 Arduino Core 3.3.7、Adafruit SHT31 Library、Arduino CLI 1.4.1

---

### Task 1: 安装 SHT30 驱动依赖

**Files:**
- User libraries: `Adafruit SHT31 Library` 及其依赖

- [x] **Step 1: 安装 Adafruit SHT31 Library**

运行：

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' lib install 'Adafruit SHT31 Library'
```

预期：命令成功安装驱动及依赖，`arduino-cli lib list` 能列出 `Adafruit SHT31 Library`。

### Task 2: SHT30 串口采集草图

**Files:**
- Create: `sht30_sensor_test/sht30_sensor_test.ino`

- [x] **Step 1: 实现完整草图**

```cpp
#include <Wire.h>
#include <Adafruit_SHT31.h>

constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;
constexpr uint8_t SHT30_ADDRESS = 0x44;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SAMPLE_INTERVAL_MS = 2000;
constexpr float MIN_TEMPERATURE_C = -40.0F;
constexpr float MAX_TEMPERATURE_C = 125.0F;
constexpr float MIN_HUMIDITY_PERCENT = 0.0F;
constexpr float MAX_HUMIDITY_PERCENT = 100.0F;

Adafruit_SHT31 sht30(&Wire);
bool sensorReady = false;

/**
 * 读取并校验一组温湿度数据，然后输出到串口。
 *
 * @param sampleCount 当前采样编号。
 * @return 数据有效时返回 true，否则返回 false。
 */
bool readAndPrintMeasurement(unsigned long sampleCount) {
  const float temperatureC = sht30.readTemperature();
  const float humidityPercent = sht30.readHumidity();

  Serial.printf("Sample #%lu\n", sampleCount);
  if (isnan(temperatureC) || isnan(humidityPercent)) {
    Serial.println("SHT30 read failed: NaN received.\n");
    return false;
  }

  if (temperatureC < MIN_TEMPERATURE_C ||
      temperatureC > MAX_TEMPERATURE_C) {
    Serial.println("SHT30 read failed: temperature out of range.\n");
    return false;
  }

  if (humidityPercent < MIN_HUMIDITY_PERCENT ||
      humidityPercent > MAX_HUMIDITY_PERCENT) {
    Serial.println("SHT30 read failed: humidity out of range.\n");
    return false;
  }

  Serial.printf("Temperature: %.1f C\n", temperatureC);
  Serial.printf("Humidity: %.1f %%\n\n", humidityPercent);
  return true;
}

/**
 * 初始化 USB 串口、I²C 总线和 SHT30。
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);

  sensorReady = sht30.begin(SHT30_ADDRESS);
  if (sensorReady) {
    Serial.println("SHT30 sensor test started at address 0x44.\n");
  } else {
    Serial.println("SHT30 initialization failed at address 0x44.");
    Serial.println("Check VCC, GND, SDA and SCL wiring.\n");
  }
}

/**
 * 每两秒执行一次温湿度采样。
 */
void loop() {
  static unsigned long sampleCount = 1;

  if (sensorReady) {
    readAndPrintMeasurement(sampleCount);
    sampleCount++;
  }
  delay(SAMPLE_INTERVAL_MS);
}
```

- [x] **Step 2: 编译验证**

运行：

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:esp32s3 --build-property build.cdc_on_boot=1 --build-path 'D:\AAAAA\smart_flower_pot\.build\sht30_sensor_test' '.\sht30_sensor_test'
```

预期：退出码为 `0`，输出 Flash 和动态内存占用信息。

- [x] **Step 3: 实机验证**

在 Arduino IDE 中保持 `USB CDC On Boot → Enabled` 并上传草图。实机采样编号连续递增至 47；温度稳定在 27.3～27.4°C，湿度受刺激后从约 77% 连续回落并稳定至 54%，未出现 `NaN` 或越界值。

### Task 3: 记录版本控制限制

- [x] **Step 1: 记录限制**

当前目录不是 Git 仓库，本功能不执行提交；初始化 Git 仓库必须另行确认。
