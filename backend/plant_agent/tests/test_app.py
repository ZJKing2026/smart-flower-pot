"""独立植物养护 Agent API 测试。"""

from unittest import TestCase
from unittest.mock import patch

from backend.plant_agent.app import PlantCareAgent


class PlantCareAgentTest(TestCase):
  """验证 Agent 的路由、RAG 与缓存。"""

  def setUp(self):
    """创建无真实密钥的测试 Agent。"""
    self.agent = PlantCareAgent({"deepseekApiKey": "test", "deepseekBaseUrl": "https://example.com", "deepseekModel": "test"})

  def testReturnsRagSourcesForSucculentQuestion(self):
    """多肉问题必须携带本地知识来源。"""
    with patch("backend.plant_agent.app.requestChat", return_value="建议选择十二卷。"):
      result = self.agent.chat("我想买个多肉，有什么推荐？")
    self.assertEqual("plant_recommendation", result["intent"])
    self.assertIn("多肉植物入门", result["sources"])

  def testRejectsUnrelatedQuestionWithoutModelCall(self):
    """无关问题不得调用模型。"""
    with patch("backend.plant_agent.app.requestChat") as requestChat:
      result = self.agent.chat("我想学习 Python 基础知识")
    self.assertEqual("out_of_scope", result["intent"])
    requestChat.assert_not_called()
