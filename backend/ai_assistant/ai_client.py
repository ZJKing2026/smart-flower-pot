"""DeepSeek 调用与养护上下文构造模块。"""

import json
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


def isFlowerpotQuestion(message: str) -> bool:
  """判断消息是否属于花盆养护或设备诊断主题。"""
  keywords = (
      "植物", "花", "浇水", "土壤", "光照", "温度", "湿度", "传感器",
      "花盆", "水泵", "继电器", "蜂鸣器", "告警", "esp32", "oled",
  )
  normalizedMessage = message.lower()
  return any(keyword in normalizedMessage for keyword in keywords)


def buildSystemPrompt(deviceData: dict) -> str:
  """根据实时设备数据构造养护助手的系统提示词。

  参数：
    deviceData: ESP32 上报的实时状态。

  返回：
    包含数据与安全限制的系统提示词。
  """
  soilAdc = deviceData.get("soilAdc", -1)
  soilMessage = "土壤数据尚未标定，不得建议自动浇水。"
  if 0 <= soilAdc < 4095:
    soilMessage = "土壤 ADC 仅为原始值，尚未换算为湿度百分比。"

  return (
      "你是智能花盆养护助手，只回答植物养护与本项目设备诊断问题。"
      "回答应分点说明数据判断依据和可执行建议，不编造数据。"
      "你只能提供建议，不能要求系统自动控制水泵。"
      f"当前数据：{json.dumps(deviceData, ensure_ascii=False)}。{soilMessage}"
  )


def requestChat(settings: dict[str, str], messages: list[dict[str, str]]) -> str:
  """调用 DeepSeek Chat Completions 接口并返回回答文本。

  参数：
    settings: DeepSeek 接口配置。
    messages: OpenAI 兼容的消息列表。

  返回：
    模型生成的回答文本。

  异常：
    RuntimeError: 密钥缺失、网络请求失败或返回格式无效时抛出。
  """
  apiKey = settings["deepseekApiKey"]
  if not apiKey:
    raise RuntimeError("未配置 DEEPSEEK_API_KEY")

  payload = json.dumps({
      "model": settings["deepseekModel"],
      "messages": messages,
  }).encode("utf-8")
  request = Request(
      f"{settings['deepseekBaseUrl'].rstrip('/')}/chat/completions",
      data=payload,
      headers={
          "Authorization": f"Bearer {apiKey}",
          "Content-Type": "application/json",
      },
      method="POST",
  )

  try:
    with urlopen(request, timeout=20) as response:
      body = json.loads(response.read().decode("utf-8"))
      return body["choices"][0]["message"]["content"].strip()
  except (HTTPError, URLError, KeyError, IndexError, TypeError, ValueError) as error:
    raise RuntimeError("DeepSeek 服务请求失败") from error
