"""设备状态与命令队列测试。"""

import json
from http.client import HTTPConnection
from threading import Thread
from unittest import TestCase
from unittest.mock import patch

from backend.ai_assistant.app import Application, DeviceStore, createServer, parseArguments


class DeviceStoreTest(TestCase):
  """验证设备状态存储行为。"""

  def testReportMarksDeviceOnlineAndReturnsLatestData(self):
    """接收上报后必须返回最新数据和在线状态。"""
    store = DeviceStore()
    store.update({"temperature": 25.2, "pump": "OFF"})

    status = store.getStatus()

    self.assertTrue(status["online"])
    self.assertEqual(25.2, status["data"]["temperature"])

  def testCommandQueueReturnsOnlyOnePendingAction(self):
    """命令被设备读取后必须从队列中移除。"""
    store = DeviceStore()
    store.queueCommand("water")

    self.assertEqual({"action": "water"}, store.takeCommand())
    self.assertEqual({"action": None}, store.takeCommand())


class ApplicationTest(TestCase):
  """验证局域网 API 的关键安全行为。"""

  def testDeviceReportMakesStatusAvailable(self):
    """设备上报后状态接口必须返回最新数据。"""
    application = Application()
    code, _ = application.handle("POST", "/api/device/report", {
        "temperature": 25.2,
        "humidity": 49.9,
        "light": 160.0,
        "soilAdc": 4095,
        "alert": "OK",
        "pump": "OFF",
    })
    statusCode, status = application.handle("GET", "/api/device/status", None)

    self.assertEqual(200, code)
    self.assertEqual(200, statusCode)
    self.assertTrue(status["online"])
    self.assertEqual("OFF", status["data"]["pump"])

  def testWaterCommandIsRejectedWhenDeviceIsOffline(self):
    """设备没有最近上报时不得创建浇水命令。"""
    application = Application()
    code, body = application.handle("POST", "/api/device/command", {"action": "water"})

    self.assertEqual(409, code)
    self.assertEqual("设备离线，不能发送浇水命令", body["error"])

  def testAdviceReportsConfigurationErrorWhenKeyIsMissing(self):
    """未配置密钥时 AI 建议接口必须返回可理解的错误。"""
    application = Application(settings={
        "deepseekApiKey": "",
        "deepseekBaseUrl": "https://api.deepseek.com",
        "deepseekModel": "deepseek-chat",
    })
    application.handle("POST", "/api/device/report", {
        "temperature": 25.2,
        "humidity": 49.9,
        "light": 160.0,
        "soilAdc": 4095,
        "alert": "OK",
        "pump": "OFF",
    })

    code, body = application.handle("POST", "/api/ai/advice", {})

    self.assertEqual(503, code)
    self.assertEqual("AI 服务未配置或暂不可用", body["error"])

  def testChatRejectsRequestWhenDeviceIsOffline(self):
    """设备离线时聊天接口必须提示无法提供实时上下文。"""
    application = Application()

    code, body = application.handle("POST", "/api/ai/chat", {"message": "现在需要浇水吗？"})

    self.assertEqual(409, code)
    self.assertEqual("设备离线，无法进行实时养护问答", body["error"])


class UnrelatedChatTest(TestCase):
  """验证无关问题不调用模型。"""

  def testRejectsWithoutCallingModel(self):
    """无关问题必须由本地直接拒绝。"""
    application = Application()
    application.handle("POST", "/api/device/report", {
        "temperature": 25.2, "humidity": 49.9, "light": 160.0,
        "soilAdc": 4095, "alert": "OK", "pump": "OFF",
    })
    with patch("backend.ai_assistant.app.requestChat") as requestChat:
      code, body = application.handle(
          "POST", "/api/ai/chat", {"message": "我想学习 Python 基础知识"}
      )

    self.assertEqual(200, code)
    self.assertIn("只处理", body["reply"])
    requestChat.assert_not_called()


class HttpServerTest(TestCase):
  """验证 HTTP 层可以承载设备状态接口。"""

  def testHttpReportAndStatusEndpoints(self):
    """网络上报后状态接口必须返回最新数据。"""
    server = createServer("127.0.0.1", 0, Application())
    worker = Thread(target=server.serve_forever, daemon=True)
    worker.start()
    connection = HTTPConnection("127.0.0.1", server.server_port, timeout=3)

    try:
      report = {
          "temperature": 25.2,
          "humidity": 49.9,
          "light": 160.0,
          "soilAdc": 4095,
          "alert": "OK",
          "pump": "OFF",
      }
      connection.request(
          "POST",
          "/api/device/report",
          body=json.dumps(report),
          headers={"Content-Type": "application/json"},
      )
      self.assertEqual(200, connection.getresponse().status)

      connection.request("GET", "/api/device/status")
      response = connection.getresponse()
      body = json.loads(response.read().decode("utf-8"))
      self.assertEqual(200, response.status)
      self.assertEqual(160.0, body["data"]["light"])
    finally:
      connection.close()
      server.shutdown()
      server.server_close()


class CommandLineTest(TestCase):
  """验证后端启动参数。"""

  def testParseArgumentsUsesRequestedHostAndPort(self):
    """命令行参数必须能配置局域网监听地址和端口。"""
    arguments = parseArguments(["--host", "0.0.0.0", "--port", "8000"])

    self.assertEqual("0.0.0.0", arguments.host)
    self.assertEqual(8000, arguments.port)

  def testHomePageReturnsMobileWeb(self):
    """根路径必须返回智能花盆手机网页。"""
    server = createServer("127.0.0.1", 0, Application())
    worker = Thread(target=server.serve_forever, daemon=True)
    worker.start()
    connection = HTTPConnection("127.0.0.1", server.server_port, timeout=3)

    try:
      connection.request("GET", "/")
      response = connection.getresponse()
      body = response.read().decode("utf-8")

      self.assertEqual(200, response.status)
      self.assertIn("智能花盆", body)
      self.assertIn("text/html", response.getheader("Content-Type"))
    finally:
      connection.close()
      server.shutdown()
      server.server_close()
