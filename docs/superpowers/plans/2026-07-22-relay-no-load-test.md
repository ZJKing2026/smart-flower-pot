# Relay No-Load Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建一个独立 Arduino 草图，验证 GPIO10 对 5V 单路继电器的低电平空载控制、有限动作次数和默认断开状态。

**Architecture:** 草图只负责继电器空载测试，不初始化传感器或显示屏。`setRelayState()` 统一转换低电平有效逻辑并输出串口状态；`setup()` 完成三轮有限测试，`loop()` 只维持关闭状态。

**Tech Stack:** Arduino C++、ESP32 Arduino Core 3.3.7、Arduino CLI 1.4.1

---

### Task 1: 创建继电器空载测试草图

**Files:**
- Create: `relay_no_load_test/relay_no_load_test.ino`

- [x] **Step 1: 写入完整测试程序**

```cpp
constexpr uint8_t RELAY_PIN = 10;
constexpr uint8_t RELAY_ON_LEVEL = LOW;
constexpr uint8_t RELAY_OFF_LEVEL = HIGH;
constexpr uint8_t TEST_CYCLE_COUNT = 3;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SERIAL_STARTUP_DELAY_MS = 1000;
constexpr unsigned long TEST_START_DELAY_MS = 3000;
constexpr unsigned long RELAY_ON_DURATION_MS = 1000;
constexpr unsigned long RELAY_OFF_DURATION_MS = 3000;
constexpr unsigned long IDLE_DELAY_MS = 1000;

/**
 * 设置继电器状态并输出当前状态。
 *
 * @param enabled true 表示吸合，false 表示释放。
 */
void setRelayState(bool enabled) {
  const uint8_t outputLevel = enabled ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL;
  digitalWrite(RELAY_PIN, outputLevel);
  Serial.printf("Relay: %s\n", enabled ? "ON" : "OFF");
}

/**
 * 初始化串口和继电器，并执行三轮空载动作测试。
 */
void setup() {
  digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);
  pinMode(RELAY_PIN, OUTPUT);

  Serial.begin(SERIAL_BAUD_RATE);
  delay(SERIAL_STARTUP_DELAY_MS);
  Serial.println("Relay no-load test started.");

  setRelayState(false);
  Serial.println("Test begins in 3 seconds.");
  delay(TEST_START_DELAY_MS);

  for (uint8_t cycle = 1; cycle <= TEST_CYCLE_COUNT; cycle++) {
    Serial.printf("Cycle %u/%u\n", cycle, TEST_CYCLE_COUNT);
    setRelayState(true);
    delay(RELAY_ON_DURATION_MS);
    setRelayState(false);
    delay(RELAY_OFF_DURATION_MS);
  }

  setRelayState(false);
  Serial.println("Relay no-load test completed. Relay remains OFF.");
}

/**
 * 持续维持继电器释放状态。
 */
void loop() {
  digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);
  delay(IDLE_DELAY_MS);
}
```

- [x] **Step 2: 检查程序安全约束**

确认草图满足以下静态条件：

- `RELAY_PIN` 为 `GPIO10`。
- `RELAY_ON_LEVEL` 为 `LOW`，`RELAY_OFF_LEVEL` 为 `HIGH`。
- GPIO 切换为输出前先写入关闭电平。
- 测试只运行 3 轮，每轮吸合时间为 1 秒。
- `loop()` 不会再次启动测试，并持续写入关闭电平。
- 草图不包含传感器、OLED、水泵或 Wi-Fi 逻辑。

- [x] **Step 3: 使用 Arduino CLI 编译草图**

运行：

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:esp32s3 --build-property build.cdc_on_boot=1 --build-path 'D:\AAAAA\smart_flower_pot\.build\relay_no_load_test' '.\relay_no_load_test'
```

预期：命令退出码为 `0`，输出程序存储空间和动态内存占用信息。

### Task 2: 完成继电器空载实机验证

**Files:**
- Test: `relay_no_load_test/relay_no_load_test.ino`
- Reference: `docs/superpowers/specs/2026-07-22-relay-no-load-test-design.md`

- [ ] **Step 1: 断电复查接线**

在 USB 断开状态下逐项确认：

```text
继电器 DC+ -> ESP32-S3 5Vin
继电器 DC- -> ESP32-S3 GND
继电器 IN  -> ESP32-S3 GPIO10
跳线帽     -> 中间针与 L 侧针
NC/COM/NO  -> 全部不接
```

预期：没有裸露导线互相接触，螺丝端子已经夹紧，继电器触点端没有负载。

- [ ] **Step 2: 上传草图**

Arduino IDE 设置：

```text
开发板: ESP32S3 Dev Module
USB CDC On Boot: Enabled
Upload Mode: UART0 / Hardware CDC
端口: 当前 ESP32-S3 对应端口
```

上传完成后，打开 `115200` 波特率串口监视器并按一次 `RST/EN`，不要按住 `BOOT`。

- [ ] **Step 3: 核对串口与机械动作**

预期串口顺序：

```text
Relay no-load test started.
Relay: OFF
Test begins in 3 seconds.
Cycle 1/3
Relay: ON
Relay: OFF
Cycle 2/3
Relay: ON
Relay: OFF
Cycle 3/3
Relay: ON
Relay: OFF
Relay: OFF
Relay no-load test completed. Relay remains OFF.
```

预期实物现象：前 3 秒保持释放；随后每轮吸合 1 秒、释放 3 秒，共出现 3 组吸合和释放声音；测试结束后保持释放，ESP32-S3 不重启。

- [ ] **Step 4: 按验收标准记录结果**

只有同时满足以下条件时，才把“继电器空载逻辑测试”记录为通过：

- 串口输出完整且顺序正确。
- 动作指示灯、吸合声与 `Relay: ON/OFF` 一致。
- 共动作 3 次，没有持续吸合。
- 测试结束后继电器保持释放。
- ESP32-S3 没有异常重启。

若任何条件不满足，立即断开 USB，保留串口输出和接线照片用于诊断，不连接水泵。

### Task 3: 记录版本控制限制

- [ ] **Step 1: 保持当前非 Git 状态**

当前目录不是 Git 仓库，本功能不执行提交。不得为了满足计划中的提交习惯而初始化 Git；初始化 Git 必须由用户另行明确确认。
