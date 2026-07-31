# ESP32 Wi-Fi 桥接测试记录

**日期：** 2026-07-24  
**草图：** `wifi_bridge_test/wifi_bridge_test.ino`  
**结果：** 实机验证通过

## 网络环境

- 手机 2.4GHz 热点：智能花盆
- 电脑后端地址：`10.220.151.30:8000`
- ESP32-S3、电脑和手机连接同一热点
- 后端启动命令：`python -m backend.ai_assistant.app --host 0.0.0.0 --port 8000`

## 编译与烧录

- 当前 PowerShell 未配置 `arduino-cli` 命令，未能在终端执行 Arduino CLI 编译。
- 使用 Arduino IDE 选择 `ESP32S3 Dev Module`、`COM8` 完成验证与上传。
- 串口监视器波特率：115200。

## 实机结果

- 串口显示 SHT30 与 BH1750 初始化成功。
- 串口出现 `WiFi connecting to 智能花盆`，随后出现 `Report HTTP: 200`。
- 手机网页与电脑网页持续显示设备在线，并显示实时传感器数据。
- 网页下发浇水命令可控制水泵。
- 水泵控制仍复用 ESP32 本地继电器控制与最长 3 秒运行保护。

## 安全限制

- `wifi_secrets.h` 为本机私密文件，不纳入草图模板，不记录热点密码。
- 当前土壤 ADC 为 4095，尚未用真实土壤标定；自动浇水继续关闭。
- 本测试只适用于可信手机热点局域网，不进行公网端口映射。
