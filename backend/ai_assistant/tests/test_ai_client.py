"""AI 上下文构造测试。"""

from unittest import TestCase

from backend.ai_assistant.ai_client import buildSystemPrompt, isFlowerpotQuestion


class FlowerpotTopicTest(TestCase):
  """验证花盆主题过滤。"""

  def testAcceptsFlowerpotQuestion(self):
    """花盆养护问题必须通过本地过滤。"""
    self.assertTrue(isFlowerpotQuestion("现在光照太低怎么办？"))

  def testRejectsProgrammingQuestion(self):
    """无关编程问题必须被本地过滤拒绝。"""
    self.assertFalse(isFlowerpotQuestion("我想学习 Python 基础知识"))


class BuildSystemPromptTest(TestCase):
  """验证 AI 养护建议的安全约束。"""

  def testMarksFullScaleSoilReadingAsUncalibrated(self):
    """满量程土壤 ADC 必须禁止 AI 建议自动浇水。"""
    prompt = buildSystemPrompt({
        "temperature": 25.2,
        "humidity": 49.9,
        "light": 160.0,
        "soilAdc": 4095,
        "alert": "OK",
        "pump": "OFF",
    })

    self.assertIn("土壤数据尚未标定", prompt)
    self.assertIn("不得建议自动浇水", prompt)
