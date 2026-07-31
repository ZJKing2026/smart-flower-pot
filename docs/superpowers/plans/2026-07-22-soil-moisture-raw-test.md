# Soil Moisture Raw ADC Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建一个独立草图，稳定采集 HW-390 土壤湿度传感器的 GPIO1 ADC 原始值和近似毫伏值。

**Architecture:** 使用 ESP32 Arduino Core 自带 ADC API，不引入第三方库。`sampleSoilSensor()` 每轮采集 20 个样本并计算平均值、最小值和最大值；主循环每两秒输出统计数据，不计算湿度百分比。

**Tech Stack:** Arduino C++、ESP32 Arduino Core 3.3.7、Arduino CLI 1.4.1

---

### Task 1: 土壤湿度 ADC 原始值草图

**Files:**
- Create: `soil_moisture_raw_test/soil_moisture_raw_test.ino`

- [x] **Step 1: 实现完整草图**

```cpp
constexpr uint8_t SOIL_SENSOR_PIN = 1;
constexpr uint8_t ADC_RESOLUTION_BITS = 12;
constexpr uint16_t ADC_MAX_VALUE = 4095;
constexpr uint8_t SAMPLE_COUNT = 20;
constexpr unsigned long SAMPLE_DELAY_MS = 5;
constexpr unsigned long OUTPUT_INTERVAL_MS = 2000;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;

struct AdcStatistics {
  uint16_t average;
  uint16_t minimum;
  uint16_t maximum;
  uint32_t millivolts;
};

/**
 * 连续采集土壤传感器 ADC，并计算统计值。
 *
 * @return 本轮 ADC 平均值、最小值、最大值和近似毫伏值。
 */
AdcStatistics sampleSoilSensor() {
  uint32_t total = 0;
  uint16_t minimum = ADC_MAX_VALUE;
  uint16_t maximum = 0;

  for (uint8_t index = 0; index < SAMPLE_COUNT; index++) {
    const uint16_t rawValue = analogRead(SOIL_SENSOR_PIN);
    total += rawValue;
    minimum = min(minimum, rawValue);
    maximum = max(maximum, rawValue);
    delay(SAMPLE_DELAY_MS);
  }

  AdcStatistics statistics;
  statistics.average = total / SAMPLE_COUNT;
  statistics.minimum = minimum;
  statistics.maximum = maximum;
  statistics.millivolts = analogReadMilliVolts(SOIL_SENSOR_PIN);
  return statistics;
}

/**
 * 初始化 USB 串口和 GPIO1 ADC 参数。
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  analogReadResolution(ADC_RESOLUTION_BITS);
  analogSetPinAttenuation(SOIL_SENSOR_PIN, ADC_11db);
  Serial.println("Soil moisture raw ADC test started.\n");
}

/**
 * 每两秒采集并输出一组 ADC 统计数据。
 */
void loop() {
  static unsigned long sampleCount = 1;
  const AdcStatistics statistics = sampleSoilSensor();

  Serial.printf("Sample #%lu\n", sampleCount);
  Serial.printf("ADC average: %u\n", statistics.average);
  Serial.printf(
    "ADC min/max: %u / %u\n",
    statistics.minimum,
    statistics.maximum
  );
  Serial.printf("Voltage: %lu mV\n\n", statistics.millivolts);

  sampleCount++;
  delay(OUTPUT_INTERVAL_MS);
}
```

- [x] **Step 2: 编译验证**

运行：

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:esp32s3 --build-property build.cdc_on_boot=1 --build-path 'D:\AAAAA\smart_flower_pot\.build\soil_moisture_raw_test' '.\soil_moisture_raw_test'
```

预期：退出码为 `0`，输出 Flash 和动态内存占用信息。

- [x] **Step 3: 实机验证**

在 Arduino IDE 中保持 `USB CDC On Boot → Enabled` 并上传草图。使用 `115200` 串口依次记录探头悬空、手握探测区域、微湿纸巾包裹探测区域和移除刺激后的数据；确认 ADC 平均值产生可重复变化，且元件区始终保持干燥。

实测结果：悬空约 `3282～3285`，微湿纸巾包裹约 `1387～1541`，移除纸巾后恢复至约 `3294～3315`，验证通过。实际土壤的干湿标定延期到取得花盆和土壤后完成。

### Task 2: 记录版本控制限制

- [x] **Step 1: 记录限制**

当前目录不是 Git 仓库，本功能不执行提交；初始化 Git 仓库必须另行确认。
