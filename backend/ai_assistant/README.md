# 智能花盆 AI 后端

该后端运行在电脑上。手机、电脑和 ESP32 连接同一个 Wi-Fi 后，手机可以查看实时数据、获得 DeepSeek 建议、聊天和发送受 ESP32 本地保护的手动浇水命令。

## 1. 准备 DeepSeek 配置

在本目录的 PowerShell 中执行：

```powershell
Copy-Item .env.example .env
```

使用记事本打开新生成的 `.env`，只填写这一项：

```dotenv
DEEPSEEK_API_KEY=你的DeepSeek_API_Key
```

不要把 `.env` 发给他人、上传到网盘或复制到 ESP32 草图中。未填写密钥时，实时状态和手动浇水界面仍可使用，AI 建议和聊天会显示“AI 服务未配置或暂不可用”。

## 2. 启动电脑端后端

在 `D:\AAAAA\smart_flower_pot` 目录打开 PowerShell，执行：

```powershell
python -m backend.ai_assistant.app --host 0.0.0.0 --port 8000
```

看到以下文本表示服务已启动：

```text
Server running at http://0.0.0.0:8000
```

保持这个 PowerShell 窗口开启。按 `Ctrl+C` 可停止服务。

## 3. 在电脑浏览器访问

打开浏览器，访问：

```text
http://127.0.0.1:8000
```

初次启动时会显示“等待设备上报”，这是正常现象；ESP32 Wi-Fi 上报将在下一阶段接入。当前可用模拟上报验证页面。

## 4. 在手机访问

1. 确认手机与电脑连接同一个 Wi-Fi，且不要使用访客网络。
2. 在电脑 PowerShell 执行：

   ```powershell
   ipconfig
   ```

3. 找到当前 Wi-Fi 网卡下的“IPv4 地址”，例如 `192.168.1.23`。
4. 在手机浏览器输入：

   ```text
   http://192.168.1.23:8000
   ```

   将示例地址替换为你电脑实际的 IPv4 地址。
5. 如果 Windows 防火墙弹窗，选择仅允许“专用网络”；不要允许公用网络。

## 5. 使用模拟数据验证网页

后端启动后，在另一个 PowerShell 窗口执行：

```powershell
$body = @{temperature=25.2; humidity=49.9; light=160.0; soilAdc=4095; alert='OK'; pump='OFF'} | ConvertTo-Json
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8000/api/device/report -ContentType 'application/json' -Body $body
```

刷新电脑或手机网页，应显示上述实时数据。由于土壤 ADC 为 `4095`，AI 会明确提示土壤数据尚未标定，不能建议自动浇水。

## 6. 局域网安全边界

- 此版本只适合可信的家庭或答辩局域网，不要将端口 8000 映射到公网。
- 手机浇水请求必须等待下一阶段 ESP32 轮询命令后才会执行；ESP32 仍会强制执行最长 3 秒保护。
- 自动浇水保持禁用，直到真实土壤完成干湿标定。
