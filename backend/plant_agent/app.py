"""独立植物养护 Agent API。"""

from backend.ai_assistant.ai_client import requestChat
from backend.ai_assistant.config import getSettings
from backend.plant_agent.cache import AnswerCache
from backend.plant_agent.retrieval import searchKnowledge
from backend.plant_agent.router import classifyIntent


OUT_OF_SCOPE_REPLY = "我是植物养护 Agent，主要解答植物选购、养护、病虫害初步排查和智能花盆设备问题。"


class PlantCareAgent:
  """组合意图路由、知识检索和大模型调用。"""

  def __init__(self, settings: dict[str, str] | None = None) -> None:
    """初始化模型配置和短时缓存。"""
    self.settings = settings or getSettings()
    self.cache = AnswerCache()

  def chat(self, question: str, deviceContext: dict | None = None) -> dict:
    """回答植物养护问题并返回意图、来源与缓存状态。"""
    intent = classifyIntent(question)
    if intent == "out_of_scope":
      return {"intent": intent, "answer": OUT_OF_SCOPE_REPLY, "sources": [], "cached": False}

    cacheKey = self.cache.buildKey(question, deviceContext)
    cachedResult = self.cache.get(cacheKey)
    if cachedResult is not None:
      return {**cachedResult, "cached": True}

    hits = searchKnowledge(question)
    sources = [hit.title for hit in hits]
    knowledgeText = "\n\n".join(hit.content for hit in hits)
    systemPrompt = (
        "你是专业植物养护 Agent。只能回答植物养护和智能花盆相关内容。"
        "使用纯文本标题和编号，不使用 Markdown 星号。"
        "基于提供的知识回答，不确定时说明需要补充的信息。"
        "不得自动控制水泵；土壤 ADC 未标定时不得建议自动浇水。"
        f"\n意图：{intent}\n知识资料：\n{knowledgeText}\n设备状态：{deviceContext or '未接入设备'}"
    )
    answer = requestChat(self.settings, [
        {"role": "system", "content": systemPrompt},
        {"role": "user", "content": question},
    ])
    result = {"intent": intent, "answer": answer, "sources": sources, "cached": False}
    self.cache.set(cacheKey, result)
    return result
