# 水泵继电器单次安全测试实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建一个独立 Arduino 草图，通过继电器让3.3V水泵仅运行2秒，随后永久关闭。

**Architecture:** 使用 GPIO10 的开漏输出控制低电平触发继电器，水泵电源经 `COM/NO` 触点切换。启动阶段和测试结束阶段均主动释放继电器，确保水泵默认关闭。

**Tech Stack:** Arduino/C++、ESP32 Arduino Core 3.3.7、Arduino CLI、ESP32-S3。

---

## 文件结构

- 创建：`pump_relay_test/pump_relay_test.ino`，包含水泵单次运行控制、串口状态输出和永久关闭逻辑。
- 修改：`docs/project-progress.md`，仅在完成实机验证后记录结果。

### Task 1: 创建水泵单次安全测试草图

**Files:**
- Create: `pump_relay_test/pump_relay_test.ino`

- [ ] **Step 1: 确认目标草图尚不存在**

Run:

```powershell
Test-Path -LiteralPath 'pump_relay_test\pump_relay_test.ino'
```

Expected: `False`

- [ ] **Step 2: 创建完整草图**

```cpp
constexpr uint8_t RELAY_PIN = 10;
constexpr uint8_t RELAY_ON_LEVEL = LOW;
constexpr uint8_t RELAY_OFF_LEVEL = HIGH;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SERIAL_STARTUP_DELAY_MS = 1000;
constexpr unsigned long TEST_START_DELAY_MS = 3000;
constexpr unsigned long PUMP_MAX_RUN_TIME_MS = 2000;
constexpr unsigned long SAFE_IDLE_DELAY_MS = 1000;

/**
 * 设置水泵状态并输出当前状态。
 *
 * @param enabled true表示启动水泵，false表示关闭水泵。
 */
void setPumpState(bool enabled) {
  const uint8_t outputLevel = enabled ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL;
  digitalWrite(RELAY_PIN, outputLevel);
  Serial.printf("Pump: %s\n", enabled ? "ON" : "OFF");
}

/**
 * 初始化串口和继电器，并执行一次限时水泵测试。
 */
void setup() {
  digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);
  pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);

  Serial.begin(SERIAL_BAUD_RATE);
  delay(SERIAL_STARTUP_DELAY_MS);
  Serial.println("Pump relay test started.");

  setPumpState(false);
  Serial.printf(
    "Pump starts in %lu seconds.\n",
    TEST_START_DELAY_MS / 1000
  );
  delay(TEST_START_DELAY_MS);

  setPumpState(true);
  delay(PUMP_MAX_RUN_TIME_MS);
  setPumpState(false);
  Serial.println("Pump relay test completed. Pump remains OFF.");
}

/**
 * 持续维持水泵关闭状态。
 */
void loop() {
  digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);
  delay(SAFE_IDLE_DELAY_MS);
}
```

- [ ] **Step 3: 静态检查安全时间和开漏配置**

Run:

```powershell
Select-String -LiteralPath 'pump_relay_test\pump_relay_test.ino' -Pattern 'OUTPUT_OPEN_DRAIN|PUMP_MAX_RUN_TIME_MS = 2000|setPumpState\(false\)'
```

Expected: 输出包含开漏配置、2000毫秒最长运行时间和关闭调用。

### Task 2: 编译验证

**Files:**
- Test: `pump_relay_test/pump_relay_test.ino`

- [ ] **Step 1: 使用工作区内构建目录编译草图**

Run:

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile `
  --fqbn esp32:esp32:esp32s3 `
  --build-path 'D:\AAAAA\smart_flower_pot\.build\pump_relay_test' `
  'D:\AAAAA\smart_flower_pot\pump_relay_test'
```

Expected: 退出码为0，并输出程序空间和动态内存占用。

- [ ] **Step 2: 交付烧录前检查表**

确认以下接线后才能烧录：

```text
DCP3512 5V   -> 继电器 DC+
DCP3512 GND  -> 继电器 DC-
DCP3512 GND  -> ESP32 GND
ESP32 GPIO10 -> 继电器 IN
DCP3512 3.3V -> 继电器 COM
继电器 NO    -> 水泵红线
水泵黑线     -> DCP3512 GND
继电器 NC    -> 不接
```

确认水泵浸入水中、软管对准容器、电子模块远离水源。

### Task 3: 烧录和实机验证

**Files:**
- Test: `pump_relay_test/pump_relay_test.ino`
- Modify after pass: `docs/project-progress.md`

- [ ] **Step 1: 使用 Arduino IDE 烧录并打开串口监视器**

Expected serial output:

```text
Pump relay test started.
Pump: OFF
Pump starts in 3 seconds.
Pump: ON
Pump: OFF
Pump relay test completed. Pump remains OFF.
```

- [ ] **Step 2: 核对实物动作**

Expected:

```text
等待3秒期间：水泵关闭
Pump: ON：继电器吸合，水泵出水
约2秒后：继电器释放，水泵停止
测试完成后：水泵不再次启动
```

- [ ] **Step 3: 记录验证结果**

仅在串口顺序和实物动作全部符合预期后，将编译占用、运行时长和观察结果追加到 `docs/project-progress.md`。若水泵未按时关闭，立即切断四路电源模块并保留现场现象，不进入自动浇水开发。

## 版本控制说明

当前工作区不是 Git 仓库，因此本计划不执行提交操作，也不擅自初始化 Git。

