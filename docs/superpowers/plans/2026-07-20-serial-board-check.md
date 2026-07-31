# ESP32-S3 Serial and Board Check Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 提供可编译、可烧录且能通过递增心跳识别异常复位的 ESP32-S3 串口最小验证程序。

**Architecture:** 草图只包含 Arduino 的 `setup()` 和 `loop()`。`setup()` 初始化串口并输出启动标识；`loop()` 按固定周期输出递增计数，不依赖任何第三方库。

**Tech Stack:** Arduino C++、ESP32 Arduino Core 3.3.7、Arduino CLI 1.4.1

---

### Task 1: 串口最小验证草图

**Files:**
- Modify: `test_minimal/test_minimal.ino`

- [x] **Step 1: 定义验收行为**

程序启动时输出 `ESP32-S3 serial check started.`，随后每秒输出 `Heartbeat: N`，其中 `N` 从 1 连续递增。

- [x] **Step 2: 实现最小草图**

使用 `SERIAL_BAUD_RATE` 和 `HEARTBEAT_INTERVAL_MS` 具名常量；在 `setup()` 中初始化串口，在 `loop()` 中输出并递增心跳计数。

- [x] **Step 3: 编译验证**

运行：

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:esp32s3 '.\test_minimal'
```

预期：命令退出码为 `0`，输出程序存储空间和动态内存占用信息。

- [x] **Step 4: 实机验证**

使用 Arduino IDE 烧录程序，打开 `115200` 波特率串口监视器。实机已观察到心跳序号从 1 连续递增至 35，串口输出正常且观察期间未发生计数归零。

- [x] **Step 5: 记录限制**

当前目录不是 Git 仓库，因此本阶段不执行提交；建立 Git 仓库需要另行获得确认。
