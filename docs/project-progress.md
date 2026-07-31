# 智能花盆项目进度记录

更新时间：2026-07-22

## 当前硬件与开发环境

- 主控：ESP32-S3
- Arduino IDE 开发板：`ESP32S3 Dev Module`
- 当前端口：`COM8`（端口号可能随 USB 重新枚举而改变）
- 串口波特率：`115200`
- `USB CDC On Boot`：`Enabled`
- `Upload Mode`：`UART0 / Hardware CDC`

## 已完成的独立功能测试

1. ESP32-S3 最小程序、编译、烧录和串口输出测试完成。
2. I²C 总线扫描测试完成：
   - OLED：`0x3C`
   - SHT30 温湿度传感器：`0x44`
   - GY-30/BH1750 光照传感器：`0x23`
3. 1.3 英寸 SH1106 OLED 独立显示测试完成。
4. SHT30 温湿度采集测试完成，温度和湿度能连续读取并正常响应环境变化。
5. GY-30/BH1750 光照采集测试完成：
   - 遮挡：小于 `10 lx`
   - 正常室内环境：约 `205 lx`
   - 手电筒照射：约 `2000 lx`
   - 移除光照或遮挡后读数能够快速恢复。
6. HW-390 V2.0.0 电容式土壤湿度传感器原始 ADC 测试完成：
   - 接线：黑线 `GND`、红线 `3V3`、黄线 `AOUT → GPIO1`
   - 悬空状态：ADC 平均值约 `3282～3285`，约 `2679～2687 mV`
   - 微湿纸巾包裹：ADC 平均值约 `1387～1541`，约 `1141～1325 mV`
   - 移除纸巾后：恢复到约 `3294～3315`
   - 结论：ADC 采集、方向性变化和恢复性均验证通过。

## 暂缓事项

- 当前没有花盆和实际土壤，因此暂不生成土壤湿度百分比。
- 获得花盆和土壤后，需要记录干土值与浇透排水后的湿土值，再进行 `0%～100%` 标定。
- 完成实际土壤标定前，不启用基于湿度百分比的自动浇水。

## 下一步

继电器模块空载测试草图已创建并编译通过，待烧录和实机观察，暂不连接水泵：

1. 已确认模块为 5V 单路继电器，使用低电平触发。
2. 已确认接线：`5Vin → DC+`、`GND → DC-`、`GPIO10 → IN`，触点端保持空载。
3. 已创建 `relay_no_load_test/relay_no_load_test.ino`，最终编译结果为 Flash `308908 bytes（23%）`、动态内存 `22088 bytes（6%）`。
4. 下一步烧录并观察三轮指示灯和继电器吸合声音。
5. 空载逻辑验证通过后，再单独设计水泵供电和最长运行时间保护。

## 当前相关文件

- `soil_moisture_raw_test/soil_moisture_raw_test.ino`
- `relay_no_load_test/relay_no_load_test.ino`
- `docs/superpowers/specs/2026-07-22-soil-moisture-raw-test-design.md`
- `docs/superpowers/plans/2026-07-22-soil-moisture-raw-test.md`

## 2026-07-23 传感器与蜂鸣告警进度

- `sensor_oled_integration_test/sensor_oled_integration_test.ino` 已完成 SHT30、BH1750、土壤 ADC 与 SH1106 OLED 的实机集成验证。
- 当前传感器接线：I2C SDA 为 GPIO8，I2C SCL 为 GPIO9，土壤 AOUT 为 GPIO1。
- 蜂鸣告警代码已加入同一草图：蜂鸣器 I/O 为 GPIO11，低电平触发；低光照阈值暂定为 50 lx，土壤 ADC 阈值暂定为 2600。
- 告警采用连续 3 轮采样确认和连续 3 轮恢复确认，每轮约 2 秒；告警期间每 2 秒短响一次。
- 2026-07-23 Arduino CLI 编译通过：程序 365264 bytes（27%），动态内存 24984 bytes（7%）。
- 实机验证通过：正常光照约 187 lx 时状态为 `Alert: OK`；遮挡后光照降至约 0.8 至 2.5 lx，系统进入 `Alert: LIGHT` 并每约 2 秒短响一次；恢复光照约 199.8 lx 后状态恢复为 `Alert: OK` 并静音。
- 蜂鸣告警阶段不接入继电器 IN 和水泵负载线，不执行任何浇水动作。

## 2026-07-23 手动浇水控制进度

- 新建草图：`manual_watering_test/manual_watering_test.ino`。
- GPIO10 使用低电平触发、开漏输出控制继电器 IN；上电后默认关闭。
- 串口监视器波特率为 115200：`W` 或 `w` 启动一次浇水，最长 3 秒；`S` 或 `s` 立即停止；运行期间重复 `W` 不会延长运行时间。
- 2026-07-23 Arduino CLI 编译通过：程序 313824 bytes（23%），动态内存 21984 bytes（6%）。
- 实机验证通过：串口已确认 `W` 启动后出现 `Pump: ON`，约 3 秒后以 `reason: timeout` 自动停止；运行期间发送 `S` 以 `reason: manual stop` 提前停止；运行中重复 `W` 输出 `Command ignored`，未延长运行时间。

## 2026-07-23 正式总集成固件

- 新建正式草图：`firmware/smart_flower_pot/smart_flower_pot.ino`。
- 已集成功能：SHT30 温湿度、BH1750 光照、土壤原始 ADC、SH1106 OLED、低光/土壤异常蜂鸣器告警、串口手动浇水和水泵最长运行时间保护。
- 自动浇水当前明确禁用；在获得花盆和实际土壤、完成干湿标定前，系统不会依据土壤读数自动启动水泵。
- 2026-07-23 Arduino CLI 编译通过：程序 `365104 bytes (27%)`，动态内存 `25000 bytes (7%)`。
- 2026-07-23 已通过 COM8 烧录至 ESP32-S3，烧录工具的数据哈希校验通过，并已自动复位。
- 待实机验证：烧录后观察 OLED 状态；遮挡光照传感器触发并恢复 `LIGHT` 告警；串口使用 `W`、`S`、`H` 分别测试定时浇水、立即停止和完整状态输出。

### 实机验收结果

- 2026-07-23 已完成 OLED 显示、温湿度与光照采集、蜂鸣器告警、串口状态查询、手动浇水、提前停止和 3 秒超时保护的实机联调，用户确认功能均可实现。
- 当前土壤 ADC 显示 `4095`，属于满量程读数；在接入真实土壤前只作为原始值展示，不用于自动浇水判断。

## 2026-07-23 DeepSeek 智能养护助手规划

- 已确定采用局域网架构：ESP32-S3、电脑 Python 后端和手机浏览器连接同一个 Wi-Fi；DeepSeek 为主模型，通义千问后续作为备用模型。
- 第一版包含手机实时状态、DeepSeek 环境建议、花盆助手聊天和受 ESP32 3 秒保护的手动浇水请求。
- 安全边界已确认：AI 仅提供建议，不能直接控制水泵；自动浇水继续禁用；API Key 只保存在电脑后端的 `.env` 中。
- 设计说明：`docs/superpowers/specs/2026-07-23-deepseek-ai-assistant-design.md`。
- 实施计划：`docs/superpowers/plans/2026-07-23-ai-backend-web.md`。
- 下次从后端与手机网页实施开始；先完成电脑端模拟设备上报、网页和 DeepSeek 接口，再单独开发 ESP32 Wi-Fi 联网草图。

## 2026-07-24 DeepSeek 后端与手机网页进度

- 已创建电脑端局域网后端：`backend/ai_assistant/app.py`，提供设备上报、状态读取、手动浇水命令队列、AI 建议与聊天接口。
- 已创建手机网页：`backend/ai_assistant/static/`，包含实时状态、AI 建议、聊天和手动浇水控制区。
- 已创建无密钥模板与部署说明：`backend/ai_assistant/.env.example`、`backend/ai_assistant/README.md`；真实 API Key 仅允许填写到用户本机创建的 `.env`，未创建也未写入任何密钥。
- 已完成后端单元测试与静态安全检查：共 11 项通过；已验证模拟数据可通过 HTTP 上报、状态接口返回在线，网页根路径返回成功。
- 已修正网页设备数据显示方式，设备上报数据以纯文本渲染，不使用 HTML 注入。
- 待执行：用户在电脑上启动后端并以手机访问局域网网页；用户配置 DeepSeek API Key 后验证 AI 建议与聊天；之后单独开发 ESP32 Wi-Fi 上报与命令轮询功能。
