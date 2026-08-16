# 智能花盆 — ESP32-S3 毕业设计

基于 ESP32-S3 的智能花盆系统，实现土壤湿度 / 温湿度 / 光照实时监测、自动灌溉、OLED 显示与 AI 养护诊断。

## 目录

- [核心特性](#核心特性)
- [技术栈](#技术栈)
- [系统架构](#系统架构)
- [项目结构](#项目结构)
- [快速开始](#快速开始)
- [开发规范](#开发规范)
- [注意事项](#注意事项)
- [License](#license)

---

## 核心特性

- **多传感器实时监测**：土壤湿度（ADC 多次采样取均值）、SHT30 温湿度、BH1750 光照强度
- **自动 / 手动浇水**：继电器控制水泵，最长 3 秒运行时间保护，串口 W/S/H 指令控制
- **智能告警**：低光照 / 土壤干燥连续 3 次确认后触发告警，蜂鸣器非阻塞短响提示
- **OLED 实时显示**：SH1106 128×64 屏显示温湿度、光照、土壤 ADC、告警与水泵状态
- **AI 养护诊断**：Flask 后端接入 DeepSeek，基于设备数据与植物知识库提供养护建议
- **安全边界**：水泵不通过 3.3V 供电，代码不含 WiFi 密码 / Token，土壤未标定时 AI 不建议自动浇水

## 技术栈

| 技术 | 说明 |
|------|------|
| ESP32-S3 | 主控（Arduino 框架） |
| C++ / Arduino | 固件开发 |
| Adafruit SHT31 / BH1750 / U8g2 | 传感器与 OLED 驱动库 |
| Flask + Python | 设备数据上报与 AI 后端 |
| DeepSeek API | AI 养护问答 |
| SQLite FTS5 | 植物知识检索（plant_agent） |
| pytest | 后端测试 |

## 系统架构

```text
┌──────────────── ESP32-S3 固件 ────────────────┐
│  SHT30(温湿度) · BH1750(光照) · 土壤湿度 ADC    │
│  SH1106 OLED · 继电器水泵(3s保护) · 蜂鸣器      │
│  WiFi 数据上报 / 串口指令                        │
└───────────────────┬───────────────────────────┘
                    │ HTTP
┌───────────────────┴───────────────────────────┐
│  Flask 后端 :8000                                │
│  ai_assistant：设备状态 · DeepSeek 建议 · 聊天    │
│  plant_agent：意图路由 · 知识检索 · 养护回答      │
└────────────────────────────────────────────────┘
```

## 项目结构

```
smart-flower-pot/
├── firmware/
│   └── smart_flower_pot/
│       └── smart_flower_pot.ino   # 主固件（传感器采集 + 水泵控制 + 告警 + OLED）
├── backend/
│   ├── ai_assistant/              # Flask Web 后端（设备上报 / DeepSeek AI / 聊天）
│   │   ├── app.py
│   │   ├── ai_client.py           # DeepSeek 客户端
│   │   ├── config.py
│   │   └── static/                # 网页前端
│   ├── plant_agent/               # 植物养护 Agent（意图路由 + 知识检索 + 缓存）
│   │   ├── app.py
│   │   ├── router.py              # 意图分类
│   │   ├── retrieval.py           # 知识检索
│   │   └── knowledge/             # 植物 / 设备知识库
│   └── tests/                     # 后端测试
├── *_test/                        # 分项硬件验证草图（I2C 扫描 / OLED / 传感器 / 继电器…）
├── docs/                          # 设计文档、接线说明、测试记录
├── scripts/                       # 数据集处理 / 模型验证脚本
├── tools/                         # 辅助脚本（热点后端配置等）
├── AGENTS.md                      # 开发规范
└── ArduinoIDE_launcher.bat        # Arduino IDE 启动脚本
```

## 快速开始

### 1. 编译烧录固件

使用 Arduino IDE 打开 `firmware/smart_flower_pot/smart_flower_pot.ino`，选择 ESP32-S3 开发板，编译上传。传感器阈值与 GPIO 引脚见代码顶部具名常量：

| 常量 | 值 | 说明 |
|------|-----|------|
| `I2C_SDA_PIN` / `I2C_SCL_PIN` | 8 / 9 | I2C 数据 / 时钟 |
| `SOIL_SENSOR_PIN` | 1 | 土壤湿度 ADC |
| `RELAY_PIN` | 10 | 水泵继电器 |
| `BUZZER_PIN` | 11 | 蜂鸣器 |
| `MAX_PUMP_RUNTIME_MS` | 3000 | 水泵最长运行时间 |
| `DRY_SOIL_THRESHOLD_ADC` | 2600 | 土壤干燥阈值 |
| `LOW_LIGHT_THRESHOLD_LX` | 50.0 | 低光照阈值 |

### 2. 启动后端

```powershell
# 配置 DeepSeek Key
cd backend/ai_assistant
Copy-Item .env.example .env   # 编辑 .env 填入 DEEPSEEK_API_KEY

# 启动服务（端口 :8000）
python -m backend.ai_assistant.app --host 0.0.0.0 --port 8000
```

浏览器访问 `http://127.0.0.1:8000` 查看实时状态；手机与电脑连同一 Wi-Fi 后可局域网访问。

### 3. 验证数据上报

```powershell
$body = @{temperature=25.2; humidity=49.9; light=160.0; soilAdc=4095; alert='OK'; pump='OFF'} | ConvertTo-Json
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8000/api/device/report -ContentType 'application/json' -Body $body
```

## 开发规范

遵循 `AGENTS.md`：功能逐项开发、编译验证、硬件安全约束。水泵不得通过 ESP32 3.3V 引脚供电；代码中不包含任何 Wi-Fi 密码或 Token，请在本地配置。

## 注意事项

- 此版本只适合可信的家庭或答辩局域网，不要将端口 8000 映射到公网
- 自动浇水保持禁用，直到真实土壤完成干湿标定
- 数据标签由大模型辅助生成，AI 建议仅供参考，不构成专业园艺或植保意见

## License

[MIT](LICENSE)
