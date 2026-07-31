# 正式总集成固件 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建可答辩演示的 ESP32-S3 本地总集成固件，提供监测、显示、告警与安全手动浇水。

**Architecture:** 新建正式草图并复用已验证的 I2C、ADC、蜂鸣器和开漏继电器控制模式。主循环以 `millis()` 调度 2 秒传感器采样、蜂鸣器短鸣、水泵超时保护和串口命令，任何情况下均不执行自动浇水。

**Tech Stack:** Arduino C++、ESP32-S3、SHT30、BH1750、SH1106/U8g2、GPIO10 继电器、GPIO11 蜂鸣器。

---

### Task 1: 创建正式固件目录和单草图实现

**Files:**
- Create: `firmware/smart_flower_pot/smart_flower_pot.ino`
- Create: `backend/ai_assistant/.gitkeep`
- Test: Arduino CLI 编译

- [ ] **Step 1: 建立正式代码层级**

创建 `firmware/smart_flower_pot/` 作为正式 ESP32 草图目录；创建 `backend/ai_assistant/` 占位目录，不放置密钥、Wi-Fi 信息或 API 代码。

- [ ] **Step 2: 实现安全硬件初始化**

在草图中定义 GPIO8、GPIO9、GPIO1、GPIO10、GPIO11，并按以下顺序初始化低电平模块：

```cpp
pinMode(RELAY_PIN, INPUT_PULLUP);
digitalWrite(RELAY_PIN, RELAY_OFF);
pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
digitalWrite(RELAY_PIN, RELAY_OFF);

pinMode(BUZZER_PIN, INPUT_PULLUP);
digitalWrite(BUZZER_PIN, HIGH);
pinMode(BUZZER_PIN, OUTPUT);
digitalWrite(BUZZER_PIN, HIGH);
```

- [ ] **Step 3: 集成传感器、OLED 与告警状态**

实现每 2 秒读取 SHT30、BH1750 和土壤 ADC；沿用 50 lx、2600 ADC、连续三轮确认规则。OLED 使用 5x8 字体显示以下六行：

```text
T:25.8C
H:49.0%
L:188.0lx
S:3227
Alert:OK
Pump:OFF
```

- [ ] **Step 4: 集成手动浇水状态机**

实现 `W/w`、`S/s`、`H/h` 命令。`W/w` 仅在水泵关闭时启动；使用 `millis()` 限制单次运行 3000 ms；`S/s` 立即调用停止逻辑；`H/h` 输出传感器、告警和水泵当前状态。水泵自动控制函数不创建、不调用。

- [ ] **Step 5: 使用 Arduino CLI 编译**

Run:

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:esp32s3 --libraries 'D:\AAAAA\smart_flower_pot\libraries' --build-path 'D:\AAAAA\smart_flower_pot\.build\smart_flower_pot' 'D:\AAAAA\smart_flower_pot\firmware\smart_flower_pot'
```

Expected: `Sketch uses ... bytes`，命令退出码为 0。

### Task 2: 记录正式固件验证要求

**Files:**
- Modify: `docs/project-progress.md`

- [ ] **Step 1: 记录编译结果与范围**

记录正式草图位置、编译结果、GPIO 定义、手动浇水 3 秒保护和“自动浇水关闭”的状态。

- [ ] **Step 2: 执行实机验证**

烧录正式草图后，确认 OLED 六行内容；遮挡 BH1750 验证蜂鸣告警；发送 `W` 验证水泵超时停止；发送 `S` 验证提前停止；发送 `H` 验证完整状态输出。
