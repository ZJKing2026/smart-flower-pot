"""本地知识库检索测试。"""

from unittest import TestCase

from backend.plant_agent.retrieval import searchKnowledge


class RetrievalTest(TestCase):
  """验证植物和设备问题能够检索到对应资料。"""

  def testFindsSucculentDocument(self):
    """多肉选购问题应返回多肉资料。"""
    hits = searchKnowledge("我想买个多肉，有什么推荐？")
    self.assertEqual("多肉植物入门", hits[0].title)

  def testFindsDeviceSafetyDocument(self):
    """土壤 ADC 问题应返回设备安全资料。"""
    hits = searchKnowledge("土壤 ADC 4095 是什么原因？")
    self.assertEqual("智能花盆设备安全", hits[0].title)
