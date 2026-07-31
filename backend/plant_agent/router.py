"""植物养护 Agent 的本地意图路由。"""


def classifyIntent(question: str) -> str:
  """根据问题内容返回植物养护 Agent 意图。"""
  normalizedQuestion = question.lower()
  if any(word in normalizedQuestion for word in ("python", "c语言", "编程", "数学", "翻译")):
    return "out_of_scope"
  if any(word in normalizedQuestion for word in ("买", "推荐", "品种", "多肉", "绿萝", "虎皮兰")):
    return "plant_recommendation"
  if any(word in normalizedQuestion for word in ("病", "虫", "黄叶", "黑斑", "烂根")):
    return "disease_check"
  if any(word in normalizedQuestion for word in ("gpio", "继电器", "水泵", "传感器", "esp32", "oled")):
    return "device_diagnosis"
  if any(word in normalizedQuestion for word in ("现在", "光照", "湿度", "温度", "浇水", "土壤")):
    return "device_environment"
  return "daily_care"
