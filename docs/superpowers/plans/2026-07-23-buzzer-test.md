# 蜂鸣器独立测试实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建一个独立 Arduino 草图，使 MH-FMD 蜂鸣器在等待3秒后短鸣3次，随后永久静音。

**Architecture:** GPIO11 以低电平触发蜂鸣器，初始化阶段使用输入上拉和输出锁存器预置避免误鸣。测试仅在 `setup()` 执行一次，`loop()` 持续维持静音状态。

**Tech Stack:** Arduino/C++、ESP32 Arduino Core 3.3.7、Arduino CLI、ESP32-S3。

---

## 文件结构

- 创建：`buzzer_test/buzzer_test.ino`，负责蜂鸣器三次短鸣及永久静音。
- 修改：`docs/project-progress.md`，仅在实机验证通过后记录结果。

### Task 1: 创建蜂鸣器独立测试草图

**Files:**
- Create: `buzzer_test/buzzer_test.ino`

- [ ] **Step 1: 确认草图尚不存在**

Run:

```powershell
Test-Path -LiteralPath 'buzzer_test\buzzer_test.ino'
```

Expected: `False`

- [ ] **Step 2: 创建完整草图**

```cpp
#include <Arduino.h>

constexpr uint8_t BUZZER_PIN = 11;
constexpr uint8_t BUZZER_ON_LEVEL = LOW;
constexpr uint8_t BUZZER_OFF_LEVEL = HIGH;
constexpr uint8_t BEEP_COUNT = 3;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SERIAL_STARTUP_DELAY_MS = 1000;
constexpr unsigned long TEST_START_DELAY_MS = 3000;
constexpr unsigned long BEEP_DURATION_MS = 300;
constexpr unsigned long BEEP_INTERVAL_MS = 300;
constexpr unsigned long SAFE_IDLE_DELAY_MS = 1000;

/**
 * 设置蜂鸣器状态并输出当前状态。
 *
 * @param enabled true表示鸣叫，false表示静音。
 */
void setBuzzerState(bool enabled) {
  const uint8_t outputLevel = enabled ? BUZZER_ON_LEVEL : BUZZER_OFF_LEVEL;
  digitalWrite(BUZZER_PIN, outputLevel);
  Serial.printf("Buzzer: %s\n", enabled ? "ON" : "OFF");
}

/**
 * 初始化串口和蜂鸣器，并执行三次短鸣测试。
 */
void setup() {
  pinMode(BUZZER_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, BUZZER_OFF_LEVEL);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, BUZZER_OFF_LEVEL);

  Serial.begin(SERIAL_BAUD_RATE);
  delay(SERIAL_STARTUP_DELAY_MS);
  Serial.println("Buzzer test started.");

  setBuzzerState(false);
  Serial.printf(
    "Buzzer test begins in %lu seconds.\n",
    TEST_START_DELAY_MS / 1000
  );
  delay(TEST_START_DELAY_MS);

  for (uint8_t beep = 1; beep <= BEEP_COUNT; beep++) {
    Serial.printf(
      "Beep %u/%u\n",
      static_cast<unsigned int>(beep),
      static_cast<unsigned int>(BEEP_COUNT)
    );
    setBuzzerState(true);
    delay(BEEP_DURATION_MS);
    setBuzzerState(false);
    delay(BEEP_INTERVAL_MS);
  }

  setBuzzerState(false);
  Serial.println("Buzzer test completed. Buzzer remains OFF.");
}

/**
 * 持续维持蜂鸣器静音状态。
 */
void loop() {
  digitalWrite(BUZZER_PIN, BUZZER_OFF_LEVEL);
  delay(SAFE_IDLE_DELAY_MS);
}
```

- [ ] **Step 3: 静态检查关键配置**

Run:

```powershell
Select-String -LiteralPath 'buzzer_test\buzzer_test.ino' -Pattern 'BUZZER_PIN = 11|BEEP_COUNT = 3|BEEP_DURATION_MS = 300|INPUT_PULLUP|BUZZER_OFF_LEVEL'
```

Expected: 输出包含 GPIO11、三次鸣叫、300毫秒、输入上拉和静音配置。

### Task 2: 编译验证

**Files:**
- Test: `buzzer_test/buzzer_test.ino`

- [ ] **Step 1: 使用 Arduino CLI 编译**

Run:

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile `
  --fqbn esp32:esp32:esp32s3 `
  --build-path 'D:\AAAAA\smart_flower_pot\.build\buzzer_test' `
  'D:\AAAAA\smart_flower_pot\buzzer_test'
```

Expected: 退出码为0，并输出程序空间及动态内存占用。

### Task 3: 烧录和实机验证

**Files:**
- Test: `buzzer_test/buzzer_test.ino`
- Modify after pass: `docs/project-progress.md`

- [ ] **Step 1: 烧录前核对接线**

```text
蜂鸣器 VCC -> ESP32 3V3
蜂鸣器 GND -> ESP32 GND
蜂鸣器 I/O -> ESP32 GPIO11
```

- [ ] **Step 2: 使用 Arduino IDE 烧录并打开115200波特率串口监视器**

Expected serial output:

```text
Buzzer test started.
Buzzer: OFF
Buzzer test begins in 3 seconds.
Beep 1/3
Buzzer: ON
Buzzer: OFF
Beep 2/3
Buzzer: ON
Buzzer: OFF
Beep 3/3
Buzzer: ON
Buzzer: OFF
Buzzer: OFF
Buzzer test completed. Buzzer remains OFF.
```

- [ ] **Step 3: 核对实物动作**

Expected:

```text
等待3秒期间保持静音
发出3次清晰短鸣
每次短鸣约300毫秒，间隔约300毫秒
测试完成后不再鸣叫
```

- [ ] **Step 4: 记录验证结果**

仅在串口顺序与实际声音均符合预期后，将编译占用和实机现象追加到 `docs/project-progress.md`。若上电即持续鸣叫，立即断开蜂鸣器 VCC 并检查引脚顺序和触发电平。

## 版本控制说明

当前工作区不是 Git 仓库，因此不执行提交操作，也不擅自初始化 Git。

