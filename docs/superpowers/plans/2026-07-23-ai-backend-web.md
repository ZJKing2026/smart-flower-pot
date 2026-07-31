# DeepSeek 智能养护助手后端与手机网页 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在电脑上运行一个无第三方依赖的 Python 局域网后端和手机网页，支持设备状态展示、DeepSeek 养护建议、聊天及受限的手动浇水命令转发。

**Architecture:** 后端使用 Python 标准库 `http.server` 提供 JSON API 与静态手机网页，在内存保存最新设备状态。DeepSeek 调用使用 `urllib.request` 的 HTTPS 请求，密钥仅由本机 `.env` 加载；当前阶段用模拟上报验证后端和网页，ESP32 Wi-Fi 上报放入下一份独立计划。

**Tech Stack:** Python 3 标准库、HTML/CSS/JavaScript、DeepSeek OpenAI 兼容 Chat Completions API、Arduino CLI（下一阶段）。

---

## 文件结构

- `backend/ai_assistant/app.py`：HTTP 路由、内存状态、命令队列、DeepSeek 请求。
- `backend/ai_assistant/config.py`：读取本机 `.env`，提供安全默认配置。
- `backend/ai_assistant/ai_client.py`：构造实时数据上下文和调用 DeepSeek。
- `backend/ai_assistant/static/index.html`：手机网页结构。
- `backend/ai_assistant/static/app.js`：状态轮询、AI 建议、聊天与浇水按钮交互。
- `backend/ai_assistant/static/style.css`：移动端优先样式。
- `backend/ai_assistant/tests/test_ai_client.py`：AI 上下文与未配置行为测试。
- `backend/ai_assistant/tests/test_app.py`：状态、命令与离线判断测试。
- `backend/ai_assistant/.env.example`：仅变量名和示例地址，禁止填入真实密钥。
- `backend/ai_assistant/README.md`：启动、手机访问和验证说明。
- `docs/superpowers/plans/2026-07-23-esp32-wifi-bridge.md`：下一阶段另建，不在本计划修改正式 ESP32 固件。

## API 约定

| 方法与路径 | 用途 | 请求或响应关键字段 |
| --- | --- | --- |
| `POST /api/device/report` | ESP32 或模拟脚本上报 | `temperature`、`humidity`、`light`、`soilAdc`、`alert`、`pump` |
| `GET /api/device/status` | 手机读取最新状态 | `online`、`updatedAt`、`data` |
| `POST /api/device/command` | 手机创建手动命令 | `action` 为 `water` 或 `stop` |
| `GET /api/device/command` | ESP32 后续轮询命令 | `action` 或 `null` |
| `POST /api/ai/advice` | 生成当前环境建议 | `advice` 或 `error` |
| `POST /api/ai/chat` | 与花盆助手聊天 | `message`，返回 `reply` 或 `error` |

### Task 1: 创建配置加载与 AI 上下文单元测试

**Files:**
- Create: `backend/ai_assistant/config.py`
- Create: `backend/ai_assistant/ai_client.py`
- Create: `backend/ai_assistant/tests/test_ai_client.py`

- [ ] **Step 1: 写出 AI 上下文测试**

```python
from unittest import TestCase

from ai_client import buildSystemPrompt


class BuildSystemPromptTest(TestCase):
    def test_marks_unavailable_soil_reading_as_uncalibrated(self):
        prompt = buildSystemPrompt({
            "temperature": 25.2,
            "humidity": 49.9,
            "light": 160.0,
            "soilAdc": 4095,
            "alert": "OK",
            "pump": "OFF",
        })

        self.assertIn("土壤数据尚未标定", prompt)
        self.assertIn("不得建议自动浇水", prompt)
```

- [ ] **Step 2: 运行测试，确认实现前失败**

Run: `python -m unittest backend.ai_assistant.tests.test_ai_client -v`

Expected: FAIL，提示无法导入 `ai_client` 或 `buildSystemPrompt`。

- [ ] **Step 3: 实现最小配置和 AI 客户端**

```python
# config.py
import os
from pathlib import Path


def loadEnvFile(envPath: Path) -> None:
    """读取本地环境变量文件，不覆盖系统中已有变量。"""
    if not envPath.exists():
        return
    for line in envPath.read_text(encoding="utf-8").splitlines():
        if not line or line.lstrip().startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        os.environ.setdefault(key.strip(), value.strip())


def getSettings() -> dict[str, str]:
    """返回后端运行所需配置。"""
    loadEnvFile(Path(__file__).with_name(".env"))
    return {
        "deepseekApiKey": os.getenv("DEEPSEEK_API_KEY", ""),
        "deepseekBaseUrl": os.getenv("DEEPSEEK_BASE_URL", "https://api.deepseek.com"),
        "deepseekModel": os.getenv("DEEPSEEK_MODEL", "deepseek-chat"),
    }
```

```python
# ai_client.py
import json
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


def buildSystemPrompt(deviceData: dict) -> str:
    """根据实时设备数据生成养护助手约束和上下文。"""
    soilMessage = "土壤数据尚未标定，不得建议自动浇水。"
    if 0 <= deviceData.get("soilAdc", -1) < 4095:
        soilMessage = "土壤 ADC 仅为原始值，尚未换算为湿度百分比。"
    return (
        "你是智能花盆养护助手。建议必须简短、可执行，不编造数据。"
        "你只能提供建议，不能要求系统自动控制水泵。"
        f"当前数据：{json.dumps(deviceData, ensure_ascii=False)}。{soilMessage}"
    )


def requestChat(settings: dict[str, str], messages: list[dict[str, str]]) -> str:
    """调用 DeepSeek Chat Completions 接口并返回文本结果。"""
    if not settings["deepseekApiKey"]:
        raise RuntimeError("未配置 DEEPSEEK_API_KEY")
    payload = json.dumps({"model": settings["deepseekModel"], "messages": messages}).encode("utf-8")
    request = Request(
        f"{settings['deepseekBaseUrl'].rstrip('/')}/chat/completions",
        data=payload,
        headers={
            "Authorization": f"Bearer {settings['deepseekApiKey']}",
            "Content-Type": "application/json",
        },
        method="POST",
    )
    try:
        with urlopen(request, timeout=20) as response:
            body = json.loads(response.read().decode("utf-8"))
    except (HTTPError, URLError) as error:
        raise RuntimeError("DeepSeek 服务请求失败") from error
    return body["choices"][0]["message"]["content"].strip()
```

- [ ] **Step 4: 运行测试确认通过**

Run: `python -m unittest backend.ai_assistant.tests.test_ai_client -v`

Expected: PASS，`test_marks_unavailable_soil_reading_as_uncalibrated` 通过。

### Task 2: 实现设备状态和命令 API

**Files:**
- Create: `backend/ai_assistant/app.py`
- Create: `backend/ai_assistant/tests/test_app.py`
- Modify: `backend/ai_assistant/ai_client.py`

- [ ] **Step 1: 写出设备状态与命令测试**

```python
from unittest import TestCase

from app import DeviceStore


class DeviceStoreTest(TestCase):
    def test_report_marks_device_online_and_returns_latest_data(self):
        store = DeviceStore()
        store.update({"temperature": 25.2, "pump": "OFF"})

        status = store.getStatus()

        self.assertTrue(status["online"])
        self.assertEqual(25.2, status["data"]["temperature"])

    def test_command_queue_returns_only_one_pending_action(self):
        store = DeviceStore()
        store.queueCommand("water")

        self.assertEqual({"action": "water"}, store.takeCommand())
        self.assertEqual({"action": None}, store.takeCommand())
```

- [ ] **Step 2: 运行测试，确认实现前失败**

Run: `python -m unittest backend.ai_assistant.tests.test_app -v`

Expected: FAIL，提示无法导入 `app` 或 `DeviceStore`。

- [ ] **Step 3: 实现状态存储与 HTTP 路由**

```python
class DeviceStore:
    """保存最新设备状态和一条待领取的手动控制命令。"""

    def __init__(self) -> None:
        self.data = None
        self.updatedAt = None
        self.pendingAction = None

    def update(self, data: dict) -> None:
        """更新设备上报的最新状态。"""
        self.data = data
        self.updatedAt = time.time()

    def getStatus(self) -> dict:
        """返回设备数据与在线状态。"""
        online = self.updatedAt is not None and time.time() - self.updatedAt <= 15
        return {"online": online, "updatedAt": self.updatedAt, "data": self.data}

    def queueCommand(self, action: str) -> None:
        """保存一条由手机创建的手动控制命令。"""
        if action not in {"water", "stop"}:
            raise ValueError("不支持的控制命令")
        self.pendingAction = action

    def takeCommand(self) -> dict:
        """读取并清除一条待执行命令。"""
        action = self.pendingAction
        self.pendingAction = None
        return {"action": action}
```

`app.py` 的请求处理器必须：

- 对 `POST /api/device/report` 校验六个状态字段后调用 `DeviceStore.update`。
- 对 `GET /api/device/status` 返回 `DeviceStore.getStatus()`。
- 对 `POST /api/device/command` 仅接受 `water` 和 `stop`；设备离线时返回 HTTP 409。
- 对 `GET /api/device/command` 返回 `DeviceStore.takeCommand()`。
- 对 `POST /api/ai/advice` 与 `POST /api/ai/chat` 捕获 `RuntimeError`，返回 HTTP 503 和中文错误文本。
- 对未知接口返回 HTTP 404；仅将 `static/` 内文件作为静态资源提供。

- [ ] **Step 4: 运行测试确认通过**

Run: `python -m unittest backend.ai_assistant.tests.test_app -v`

Expected: PASS，两个 `DeviceStoreTest` 通过。

### Task 3: 创建手机网页和本地交互验证

**Files:**
- Create: `backend/ai_assistant/static/index.html`
- Create: `backend/ai_assistant/static/style.css`
- Create: `backend/ai_assistant/static/app.js`

- [ ] **Step 1: 创建移动端页面结构**

```html
<main class="page">
  <header><h1>智能花盆</h1><p id="deviceState">等待设备上报</p></header>
  <section class="card"><h2>实时状态</h2><div id="metrics"></div></section>
  <section class="card"><h2>AI 养护建议</h2><button id="adviceButton">分析当前环境</button><p id="adviceText">尚未分析</p></section>
  <section class="card"><h2>花盆助手</h2><div id="chatLog"></div><textarea id="chatInput" placeholder="例如：现在需要浇水吗？"></textarea><button id="chatButton">发送</button></section>
  <section class="card"><h2>手动浇水</h2><button id="waterButton">开始浇水</button><button id="stopButton">停止浇水</button><p>水泵由 ESP32 限制，最长运行 3 秒。</p></section>
</main>
```

- [ ] **Step 2: 实现状态轮询和操作请求**

```javascript
async function requestJson(url, options = {}) {
  const response = await fetch(url, options);
  const body = await response.json();
  if (!response.ok) throw new Error(body.error || "请求失败");
  return body;
}

async function refreshStatus() {
  const status = await requestJson("/api/device/status");
  document.querySelector("#deviceState").textContent = status.online ? "设备在线" : "设备离线";
  const data = status.data || {};
  document.querySelector("#metrics").textContent =
    `温度 ${data.temperature ?? "--"} °C｜湿度 ${data.humidity ?? "--"} %｜光照 ${data.light ?? "--"} lx｜土壤 ADC ${data.soilAdc ?? "--"}｜告警 ${data.alert ?? "--"}｜水泵 ${data.pump ?? "--"}`;
}

setInterval(refreshStatus, 3000);
refreshStatus();
```

按钮事件必须使用 `/api/ai/advice`、`/api/ai/chat` 和 `/api/device/command`，所有失败信息显示在对应页面区域，不能只写入浏览器控制台。

- [ ] **Step 3: 本地启动后端并进行 API 冒烟测试**

Run: `python backend/ai_assistant/app.py --host 0.0.0.0 --port 8000`

Expected: 输出 `Server running at http://0.0.0.0:8000`。

另开 PowerShell：

```powershell
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8000/api/device/report -ContentType 'application/json' -Body '{"temperature":25.2,"humidity":49.9,"light":160.0,"soilAdc":4095,"alert":"OK","pump":"OFF"}'
Invoke-RestMethod http://127.0.0.1:8000/api/device/status
```

Expected: 第二条命令返回 `online: True` 和刚上报的字段；手机访问 `http://电脑局域网IP:8000` 可看到页面数据。

### Task 4: 添加本机配置模板和使用文档

**Files:**
- Create: `backend/ai_assistant/.env.example`
- Create: `backend/ai_assistant/README.md`

- [ ] **Step 1: 创建无密钥配置模板**

```dotenv
DEEPSEEK_API_KEY=
DEEPSEEK_BASE_URL=https://api.deepseek.com
DEEPSEEK_MODEL=deepseek-chat
```

- [ ] **Step 2: 编写运行说明**

`README.md` 必须包含：

- 将 `.env.example` 复制为 `.env`，仅在 `.env` 中填写 DeepSeek API Key。
- 运行 `python app.py --host 0.0.0.0 --port 8000`。
- 使用 `ipconfig` 找到电脑 IPv4 地址，手机访问 `http://该地址:8000`。
- Windows 防火墙提示时只允许“专用网络”。
- 未配置 API Key 时，实时状态和浇水接口可用，AI 接口显示未配置。
- 不得将 `.env` 上传、分享或复制进 ESP32 草图。

- [ ] **Step 3: 运行全量单元测试**

Run: `python -m unittest discover -s backend/ai_assistant/tests -v`

Expected: 所有测试 PASS。

## 后续独立计划：ESP32 Wi-Fi 桥接

完成本计划并由电脑/手机端验证后，再为 ESP32 创建独立的 Wi-Fi 桥接草图和测试计划。该阶段需要用户提供 Wi-Fi 名称与密码，并在创建真实 `.env` 前获得明确授权；不修改当前已经实机验证的正式固件，直到新的联网草图单独编译和烧录验证通过。

## 自检结果

- 规格覆盖：实时状态、AI 建议、聊天、手机手动浇水、离线显示、密钥隔离和自动浇水禁用均有对应任务。
- 范围控制：本计划不修改 ESP32 固件，避免后端、网页和无线硬件同时引入故障。
- 占位检查：计划中没有待补内容或未定义的实现步骤。
