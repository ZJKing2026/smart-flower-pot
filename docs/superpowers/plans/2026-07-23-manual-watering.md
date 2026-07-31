# 手动浇水控制 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新建独立串口手动浇水草图，支持固定 3 秒浇水和随时停止。

**Architecture:** 使用 GPIO10 的低电平触发、开漏继电器输出。主循环持续读取串口字符，并用 `millis()` 检查水泵最长运行时间，确保 `S` 可立即执行且任意单次运行不超过 3 秒。

**Tech Stack:** Arduino C++、ESP32-S3、GPIO10、低电平触发 5V 继电器、DCP3512 独立供电水泵。

---

### Task 1: 创建安全的串口手动浇水草图

**Files:**
- Create: `manual_watering_test/manual_watering_test.ino`
- Test: Arduino CLI 编译

- [ ] **Step 1: 定义硬件和安全常量**

创建草图并定义 GPIO10、低电平继电器状态、115200 波特率、3 秒最大运行时间。

```cpp
constexpr uint8_t RELAY_PIN = 10;
constexpr uint8_t RELAY_ON = LOW;
constexpr uint8_t RELAY_OFF = HIGH;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SERIAL_STARTUP_DELAY_MS = 1000;
constexpr unsigned long MAX_PUMP_RUNTIME_MS = 3000;
```

- [ ] **Step 2: 实现继电器安全初始化和状态切换函数**

使用以下顺序初始化，确保 ESP32-S3 启动时继电器保持关闭：

```cpp
pinMode(RELAY_PIN, INPUT_PULLUP);
digitalWrite(RELAY_PIN, RELAY_OFF);
pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
digitalWrite(RELAY_PIN, RELAY_OFF);
```

实现 `startPump()`、`stopPump(const char *reason)` 和 `printHelp()`，所有状态变化输出到串口。

- [ ] **Step 3: 实现非阻塞命令处理和超时停止**

`loop()` 读取串口中的 `W/w/S/s`；仅在水泵停止时允许 `W` 启动，运行中重复 `W` 输出忽略提示；每次循环检查 `millis() - pumpStartedAt >= MAX_PUMP_RUNTIME_MS` 并调用 `stopPump("timeout")`。

- [ ] **Step 4: 使用 Arduino CLI 编译**

Run:

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:esp32s3 --build-path 'D:\AAAAA\smart_flower_pot\.build\manual_watering_test' 'D:\AAAAA\smart_flower_pot\manual_watering_test'
```

Expected: `Sketch uses ... bytes`，命令退出码为 0。

### Task 2: 记录并执行实机验证

**Files:**
- Modify: `docs/project-progress.md`

- [ ] **Step 1: 记录编译信息和接线安全条件**

写入草图名称、GPIO10、3 秒保护、继电器与水泵由 DCP3512 供电的条件，以及待完成的实机验证。

- [ ] **Step 2: 验证固定时长和手动停止**

烧录后打开 115200 串口监视器：发送 `W`，确认水泵工作后约 3 秒自动停止；再次发送 `W` 后立即发送 `S`，确认水泵提前停止。验证完成前不接入自动浇水代码。
