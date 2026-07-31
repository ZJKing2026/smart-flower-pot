# ESP32 Wi-Fi 桥接实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建一个独立 ESP32-S3 Wi-Fi 桥接测试草图，将花盆状态持续上传到本机后端，并安全执行网页下发的手动浇水/停止命令。

**Architecture:** `wifi_bridge_test` 复制已经实机验证的总集成控制逻辑，并新增一个非阻塞网络层。传感器采样保持 2 秒周期；网络层以 5 秒上报状态、1 秒轮询命令。Wi-Fi 连接信息位于不纳入源码的 `wifi_secrets.h`，水泵命令始终复用本地 3 秒超时保护。

**Tech Stack:** Arduino C++、ESP32 Arduino Core、WiFi.h、HTTPClient.h、Arduino CLI、现有 SHT30/BH1750/U8g2 库、Python 标准库后端。

---

## 文件结构

- `wifi_bridge_test/wifi_bridge_test.ino`：独立的 Wi-Fi 桥接硬件测试草图。
- `wifi_bridge_test/wifi_secrets.h.example`：不含真实密码的本地配置模板。
- `wifi_bridge_test/.gitignore`：忽略 `wifi_secrets.h`。
- `docs/wifi-bridge-test-result-2026-07-24.md`：编译与实机测试记录。
- `docs/project-progress.md`：项目进度追加记录。

### Task 1: 建立私密配置边界

**Files:**
- Create: `wifi_bridge_test/wifi_secrets.h.example`
- Create: `wifi_bridge_test/.gitignore`

- [ ] **Step 1: 创建不含真实凭据的配置模板**

```cpp
#pragma once

constexpr char WIFI_SSID[] = "replace-with-2.4ghz-hotspot-name";
constexpr char WIFI_PASSWORD[] = "replace-with-hotspot-password";
constexpr char BACKEND_HOST[] = "10.220.151.30";
constexpr uint16_t BACKEND_PORT = 8000;
```

- [ ] **Step 2: 忽略真实凭据文件**

```gitignore
wifi_secrets.h
```

- [ ] **Step 3: 由用户本机创建真实配置**

Run in PowerShell:

```powershell
Copy-Item wifi_bridge_test\wifi_secrets.h.example wifi_bridge_test\wifi_secrets.h
notepad wifi_bridge_test\wifi_secrets.h
```

Expected: 用户只在本机将 `WIFI_SSID` 改为 `智能花盆` 并填写热点密码；不把密码发送到聊天或写入示例文件。

### Task 2: 实现独立桥接草图

**Files:**
- Create: `wifi_bridge_test/wifi_bridge_test.ino`
- Reference: `firmware/smart_flower_pot/smart_flower_pot.ino`

- [ ] **Step 1: 复制已验证的本地功能边界**

保留 GPIO：I2C SDA=8、SCL=9、土壤 ADC=1、继电器=10（低电平有效开漏）、蜂鸣器=11（低电平有效）；保留 SHT30、BH1750、OLED、串口 `W/S/H`、告警和水泵 3000ms 超时保护。

- [ ] **Step 2: 增加 Wi-Fi 状态机**

实现 `startWifiConnection()` 与 `updateWifi()`：

```cpp
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 10000;

void startWifiConnection() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void updateWifi() {
  static unsigned long lastRetryAt = 0;
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  if (millis() - lastRetryAt >= WIFI_RETRY_INTERVAL_MS) {
    lastRetryAt = millis();
    WiFi.disconnect();
    startWifiConnection();
  }
}
```

- [ ] **Step 3: 增加状态上报函数**

实现 `reportDeviceStatus()`：仅在 `WiFi.status() == WL_CONNECTED` 且有效传感器数据已产生时执行，构造 JSON 后调用：

```cpp
HTTPClient http;
http.begin(client, reportUrl);
http.addHeader("Content-Type", "application/json");
const int statusCode = http.POST(payload);
http.end();
```

其中 `reportUrl` 固定为 `http://<BACKEND_HOST>:<BACKEND_PORT>/api/device/report`，字段严格使用 `temperature`、`humidity`、`light`、`soilAdc`、`alert`、`pump`。

- [ ] **Step 4: 增加网页命令轮询函数**

实现 `pollDeviceCommand()`，每 1000ms 调用 `GET /api/device/command`。解析响应中的 `action`：`water` 调用 `startPump()`，`stop` 调用 `stopPump("web command")`，其它动作仅记录并忽略。

- [ ] **Step 5: 让网络周期与本地循环并存**

在 `loop()` 中持续执行以下调度，所有分支均不使用 `delay()` 等待网络：

```cpp
updateWifi();
updatePumpTimeout();
updateBuzzer(currentAlertState);
updateNetworkTasks();
updateSensorSampling();
```

Expected: Wi-Fi 或后端不可用时，本地 OLED、串口、蜂鸣器和水泵超时保护仍持续运行。

### Task 3: 编译验证

**Files:**
- Verify: `wifi_bridge_test/wifi_bridge_test.ino`

- [ ] **Step 1: 编译独立草图**

Run:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3 wifi_bridge_test
```

Expected: 编译完成且没有错误；如果提示缺少 `wifi_secrets.h`，只创建本机私密文件，不修改示例文件。

- [ ] **Step 2: 检查真实凭据未被误纳入可见文件**

Run:

```powershell
Get-ChildItem wifi_bridge_test -Force
```

Expected: 可见 `.gitignore` 与 `wifi_secrets.h.example`；真实 `wifi_secrets.h` 仅本机使用，内容不显示在终端或聊天中。

### Task 4: 实机联网验证

**Files:**
- Create: `docs/wifi-bridge-test-result-2026-07-24.md`
- Modify: `docs/project-progress.md`

- [ ] **Step 1: 烧录前检查**

确认手机热点“智能花盆”为 2.4GHz，运行后端的电脑 IP 为 `10.220.151.30`，并保持后端窗口打开：

```powershell
python -m backend.ai_assistant.app --host 0.0.0.0 --port 8000
```

- [ ] **Step 2: 烧录测试草图**

Run:

```powershell
arduino-cli upload -p COM8 --fqbn esp32:esp32:esp32s3 wifi_bridge_test
```

Expected: 上传成功；串口以 115200 显示 Wi-Fi IP、状态上报 HTTP 状态码和命令轮询结果。

- [ ] **Step 3: 验证持续在线**

在手机网页和电脑网页保持打开至少 30 秒。

Expected: 页面持续显示设备在线，温湿度、光照、土壤 ADC、告警和水泵状态保持刷新；不会在 15 秒后离线。

- [ ] **Step 4: 验证网页手动浇水安全性**

从网页发送浇水命令，观察继电器/水泵；再发送停止命令。

Expected: 浇水命令在约 1 秒内生效；无停止命令时最多运行 3 秒；停止命令能立即关闭水泵。

- [ ] **Step 5: 验证断网降级**

临时关闭手机热点或停止后端，观察 OLED、串口 `H`、低光告警和 `W/S` 控制。

Expected: 联网报错或重连日志出现，但本地功能不停止，水泵仍被 3 秒保护。

- [ ] **Step 6: 记录结果**

在 `docs/wifi-bridge-test-result-2026-07-24.md` 记录编译命令、上传结果、持续在线时间、网页浇水结果、断网降级结果和未完成项；在 `docs/project-progress.md` 追加本次桥接测试状态。

## 自检

- 设计中的 5 秒上报、1 秒轮询、2.4GHz 限制、私密凭据、本地 3 秒保护和断网降级均有对应任务。
- 本计划不改动正式总程序集、不变更硬件接线、不启用自动浇水。
- 所有路径、命令、接口字段和预期结果已明确。
