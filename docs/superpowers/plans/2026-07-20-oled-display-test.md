# SH1106 OLED Display Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建一个独立草图，在 SH1106 OLED 上显示完整边框、固定信息和每秒递增计数。

**Architecture:** 使用 U8g2 的 SH1106 128×64 全缓冲硬件 I²C 驱动。草图显式初始化 GPIO8/GPIO9 和 `0x3C` 地址；`renderTestScreen()` 负责单帧绘制，主循环每秒更新计数并同步输出到串口。

**Tech Stack:** Arduino C++、ESP32 Arduino Core 3.3.7、U8g2、Arduino CLI 1.4.1

---

### Task 1: 安装 OLED 驱动依赖

**Files:**
- User library: `U8g2`

- [x] **Step 1: 安装 U8g2**

运行：

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' lib install U8g2
```

预期：命令成功安装 U8g2，`arduino-cli lib list` 能够列出该库及版本。

### Task 2: OLED 固定画面草图

**Files:**
- Create: `oled_display_test/oled_display_test.ino`

- [x] **Step 1: 实现完整草图**

```cpp
#include <Wire.h>
#include <U8g2lib.h>

constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long REFRESH_INTERVAL_MS = 1000;

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(
  U8G2_R0,
  U8X8_PIN_NONE
);

/**
 * 绘制 OLED 固定测试信息和当前刷新计数。
 *
 * @param refreshCount 当前画面刷新次数。
 */
void renderTestScreen(unsigned long refreshCount) {
  oled.clearBuffer();
  oled.drawFrame(0, 0, 128, 64);
  oled.setFont(u8g2_font_6x10_tf);
  oled.setCursor(4, 12);
  oled.print("Smart Flower Pot");
  oled.setCursor(4, 27);
  oled.print("OLED Test");
  oled.setCursor(4, 42);
  oled.print("Address: 0x3C");
  oled.setCursor(4, 57);
  oled.print("Status: OK #");
  oled.print(refreshCount);
  oled.sendBuffer();
}

/**
 * 初始化 USB 串口、I²C 总线和 SH1106 OLED。
 */
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);
  oled.setI2CAddress(OLED_ADDRESS << 1);
  oled.begin();
  Serial.println("SH1106 OLED display test started.");
}

/**
 * 每秒刷新一次 OLED 测试画面和串口计数。
 */
void loop() {
  static unsigned long refreshCount = 1;

  renderTestScreen(refreshCount);
  Serial.printf("OLED refresh: %lu\n", refreshCount);
  refreshCount++;
  delay(REFRESH_INTERVAL_MS);
}
```

- [x] **Step 2: 编译验证**

运行：

```powershell
& 'D:\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:esp32s3 --build-property build.cdc_on_boot=1 --build-path 'D:\AAAAA\smart_flower_pot\.build\oled_display_test' '.\oled_display_test'
```

预期：退出码为 `0`，输出 Flash 和动态内存占用信息。

- [x] **Step 3: 实机验证**

在 Arduino IDE 中保持 `USB CDC On Boot → Enabled` 并上传草图。实机照片确认屏幕显示完整边框、四行内容和 `#63` 刷新计数；画面方向、地址、边缘和持续刷新均正常。

### Task 3: 记录版本控制限制

- [x] **Step 1: 记录限制**

当前目录不是 Git 仓库，本功能不执行提交；初始化 Git 仓库必须另行确认。
