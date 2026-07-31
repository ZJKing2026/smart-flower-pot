# 手机热点网络配置

## 固定的本机服务

- 智能花盆后端：`0.0.0.0:8000`
- 植境 Agent：电脑本机 `127.0.0.1:8100`
- PlantCare-VL 模型：电脑本机 `127.0.0.1:8080`

植境 Agent 和模型均运行在同一台电脑，不随手机热点的 IP 变化而修改。

## 热点切换后的操作

1. 让电脑、ESP32 和手机连接同一个 2.4GHz 手机热点。
2. 在项目根目录运行：

```powershell
.\tools\set_hotspot_backend_host.ps1
```

脚本会显示电脑当前建议使用的 IPv4 地址与手机访问地址，但不会修改文件。

3. 确认地址后运行：

```powershell
.\tools\set_hotspot_backend_host.ps1 -Apply
```

该命令只更新 `wifi_bridge_test\wifi_secrets.h` 中的 `BACKEND_HOST`，不会输出或修改 Wi-Fi 密码。

4. 在 Arduino IDE 中重新烧录 `wifi_bridge_test`。
5. 在电脑启动花盆后端：

```powershell
python -m backend.ai_assistant.app --host 0.0.0.0 --port 8000
```

6. 使用脚本显示的 `http://电脑IP:8000` 在手机浏览器打开花盆页面。

## 手动指定地址

当电脑同时连接多个网络、自动识别结果不正确时，可指定热点 IPv4：

```powershell
.\tools\set_hotspot_backend_host.ps1 -HostAddress 10.220.151.30 -Apply
```

## 连接关系

```text
ESP32、手机 -- 手机热点 --> 电脑IPv4:8000
电脑花盆后端 -- 本机 --> 127.0.0.1:8100 植境 Agent
植境 Agent -- 本机 --> 127.0.0.1:8080 PlantCare-VL
```
