"""意图路由和缓存测试。"""

from unittest import TestCase

from backend.plant_agent.cache import AnswerCache
from backend.plant_agent.router import classifyIntent


class RouterTest(TestCase):
  """验证植物问题路由到正确工作流。"""

  def testClassifiesSucculentPurchase(self):
    """多肉购买问题应进入植物推荐流程。"""
    self.assertEqual("plant_recommendation", classifyIntent("我想买个多肉，有什么推荐？"))

  def testRejectsProgrammingQuestion(self):
    """明确无关的编程问题应被拒绝。"""
    self.assertEqual("out_of_scope", classifyIntent("我想学习 Python 基础知识"))

  def testCachesSameQuestionAndDeviceState(self):
    """相同问题和设备状态应命中缓存。"""
    cache = AnswerCache()
    key = cache.buildKey("现在需要浇水吗？", {"soilAdc": 4095})
    cache.set(key, {"answer": "请先人工检查土壤。"})
    self.assertEqual("请先人工检查土壤。", cache.get(key)["answer"])
