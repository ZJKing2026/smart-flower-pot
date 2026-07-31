"""手机网页静态安全检查。"""

from pathlib import Path
from unittest import TestCase


class StaticSecurityTest(TestCase):
  """验证网页不会把设备数据作为 HTML 注入。"""

  def testDeviceMetricsDoNotUseInnerHtml(self):
    """指标渲染必须使用文本节点而非 innerHTML。"""
    scriptPath = Path(__file__).parents[1] / "static" / "app.js"
    script = scriptPath.read_text(encoding="utf-8")

    self.assertNotIn("innerHTML", script)
    self.assertIn("textContent", script)
