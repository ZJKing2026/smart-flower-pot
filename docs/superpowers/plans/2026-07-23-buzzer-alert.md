# 蜂鸣告警功能 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在传感器与 OLED 集成草图中加入低光照和土壤偏干的间歇蜂鸣告警。

**Architecture:** 在现有采样结果基础上维护光照与土壤的异常连续计数。达到三轮后更新稳定告警状态；蜂鸣器依据该状态和 `millis()` 非阻塞定时器发出短鸣，OLED 和串口显示同一告警状态。

**Tech Stack:** Arduino C++、ESP32-S3、BH1750、Adafruit SHT31、U8g2、GPIO11 低电平触发蜂鸣器。

---

### Task 1: 在集成草图中实现告警状态与蜂鸣器控制

**Files:**
- Modify: `sensor_oled_integration_test/sensor_oled_integration_test.ino`
- Test: Arduino CLI 编译

- [ ] **Step 1: 添加硬件与阈值常量**

在现有 GPIO 和时间常量区添加以下常量：

```cpp
constexpr uint8_t BUZZER_PIN = 11;
constexpr uint16_t LOW_LIGHT_THRESHOLD_LX = 50;
constexpr uint16_t DRY_SOIL_THRESHOLD_ADC = 2600;
constexpr uint8_t ALERT_CONFIRM_COUNT = 3;
constexpr unsigned long BUZZER_INTERVAL = 2000;
constexpr unsigned long BUZZER_DURATION = 200;
```

- [ ] **Step 2: 添加告警数据模型和防抖判定函数**

新增 `AlertState` 枚举、异常连续计数器以及 `updateAlertState()`。当传感器读数无效时，不增加异常计数；连续三轮正常才清除对应告警。

```cpp
enum class AlertState : uint8_t {
  OK,
  LIGHT,
  SOIL,
  BOTH
};
```

- [ ] **Step 3: 添加低电平蜂鸣器非阻塞控制函数**

新增 `initializeBuzzer()` 和 `updateBuzzer(AlertState state)`：初始化时以 `INPUT_PULLUP`、高电平、`OUTPUT`、高电平的顺序保持静音；告警时每 2000 ms 拉低 GPIO11 200 ms，其余时间保持高电平。

- [ ] **Step 4: 修改 OLED 和串口输出**

将 OLED 调整为五行紧凑布局，显示温度、湿度、光照、土壤 ADC 和 `Alert: <state>`。在串口每轮数据末尾追加 `Alert: <state>`。

- [ ] **Step 5: 将告警流程接入主循环**

`loop()` 每轮读取传感器后依次调用 `updateAlertState(data)`、`printSensorData(...)`、`displaySensorData(...)`、`updateBuzzer(...)`。初始化流程在 ADC 设置后调用 `initializeBuzzer()`。

- [ ] **Step 6: 使用 Arduino CLI 编译**

Run:

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:esp32s3 --libraries 'D:\AAAAA\smart_flower_pot\libraries' --build-path 'D:\AAAAA\smart_flower_pot\.build\sensor_oled_integration_test' 'D:\AAAAA\smart_flower_pot\sensor_oled_integration_test'
```

Expected: `Sketch uses ... bytes`，命令退出码为 0。

### Task 2: 记录编译结果与实机验证步骤

**Files:**
- Modify: `docs/project-progress.md`

- [ ] **Step 1: 记录编译结果**

记录草图名称、GPIO11、临时阈值、编译命令结果以及尚未完成的实机验证。

- [ ] **Step 2: 执行实机验证**

保持继电器 IN 和水泵负载线断开。烧录后先确认明亮环境静音，再遮挡 BH1750 超过 6 秒，确认 OLED 显示 `Alert: LIGHT` 且蜂鸣器每约 2 秒短响；移开遮挡物超过 6 秒，确认恢复静音。
