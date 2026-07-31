"""智能花盆局域网后端入口。"""

import argparse
import json
import mimetypes
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from backend.ai_assistant.ai_client import buildSystemPrompt, isFlowerpotQuestion, requestChat
from backend.ai_assistant.config import getSettings


class DeviceStore:
  """保存最新设备状态和一条待领取的手动控制命令。"""

  def __init__(self) -> None:
    """初始化空设备状态和命令队列。"""
    self.data = None
    self.updatedAt = None
    self.pendingAction = None

  def update(self, data: dict) -> None:
    """更新设备上报的最新状态。

    参数：
      data: 设备上报的状态字典。
    """
    self.data = data
    self.updatedAt = time.time()

  def getStatus(self) -> dict:
    """返回最新状态和设备在线状态。

    返回：
      包含在线状态、更新时间和数据的字典。
    """
    online = self.updatedAt is not None and time.time() - self.updatedAt <= 15
    return {
        "online": online,
        "updatedAt": self.updatedAt,
        "data": self.data,
    }

  def queueCommand(self, action: str) -> None:
    """保存一条待由 ESP32 领取的控制命令。

    参数：
      action: 允许的水泵控制命令。

    异常：
      ValueError: 命令不是 water 或 stop 时抛出。
    """
    if action not in {"water", "stop"}:
      raise ValueError("不支持的控制命令")
    self.pendingAction = action

  def takeCommand(self) -> dict:
    """读取并清除一条待执行命令。

    返回：
      包含 action 的命令字典；无命令时 action 为 None。
    """
    action = self.pendingAction
    self.pendingAction = None
    return {"action": action}


class Application:
  """处理手机和 ESP32 使用的 JSON API。"""

  REQUIRED_REPORT_FIELDS = {
      "temperature",
      "humidity",
      "light",
      "soilAdc",
      "alert",
      "pump",
  }

  def __init__(self, settings: dict[str, str] | None = None) -> None:
    """初始化设备状态容器和 AI 接口配置。

    参数：
      settings: 可选的 DeepSeek 配置，未传入时从本机环境读取。
    """
    self.deviceStore = DeviceStore()
    self.settings = settings if settings is not None else getSettings()

  def handle(self, method: str, path: str, body: dict | None) -> tuple[int, dict]:
    """分发 JSON API 请求。

    参数：
      method: HTTP 请求方法。
      path: HTTP 请求路径。
      body: 已解析的 JSON 请求体；GET 请求可为 None。

    返回：
      HTTP 状态码和 JSON 响应内容。
    """
    if method == "POST" and path == "/api/device/report":
      return self._handleDeviceReport(body)
    if method == "GET" and path == "/api/device/status":
      return 200, self.deviceStore.getStatus()
    if method == "POST" and path == "/api/device/command":
      return self._handleDeviceCommand(body)
    if method == "GET" and path == "/api/device/command":
      return 200, self.deviceStore.takeCommand()
    if method == "POST" and path == "/api/ai/advice":
      return self._handleAdvice()
    if method == "POST" and path == "/api/ai/chat":
      return self._handleChat(body)
    return 404, {"error": "接口不存在"}

  def _handleDeviceReport(self, body: dict | None) -> tuple[int, dict]:
    """校验并保存 ESP32 的状态上报。"""
    if not isinstance(body, dict) or not self.REQUIRED_REPORT_FIELDS.issubset(body):
      return 400, {"error": "设备上报字段不完整"}

    report = {field: body[field] for field in self.REQUIRED_REPORT_FIELDS}
    self.deviceStore.update(report)
    return 200, {"message": "状态已更新"}

  def _handleDeviceCommand(self, body: dict | None) -> tuple[int, dict]:
    """校验手机发起的水泵控制命令。"""
    if not self.deviceStore.getStatus()["online"]:
      return 409, {"error": "设备离线，不能发送浇水命令"}
    if not isinstance(body, dict):
      return 400, {"error": "命令格式无效"}

    try:
      self.deviceStore.queueCommand(body.get("action", ""))
    except ValueError:
      return 400, {"error": "仅支持 water 或 stop 命令"}
    return 200, {"message": "命令已排队"}

  def _handleAdvice(self) -> tuple[int, dict]:
    """根据最新设备数据生成一条 AI 养护建议。"""
    status = self.deviceStore.getStatus()
    if not status["online"]:
      return 409, {"error": "设备离线，无法分析当前环境"}

    messages = [
        {"role": "system", "content": buildSystemPrompt(status["data"])},
        {"role": "user", "content": "请按数据解读、风险判断、建议操作、注意事项四个部分分析当前环境。"},
    ]
    try:
      advice = requestChat(self.settings, messages)
    except RuntimeError:
      return 503, {"error": "AI 服务未配置或暂不可用"}
    return 200, {"advice": advice}

  def _handleChat(self, body: dict | None) -> tuple[int, dict]:
    """携带实时状态上下文处理用户的养护问题。"""
    status = self.deviceStore.getStatus()
    if not status["online"]:
      return 409, {"error": "设备离线，无法进行实时养护问答"}
    if not isinstance(body, dict) or not isinstance(body.get("message"), str):
      return 400, {"error": "聊天消息格式无效"}

    message = body["message"].strip()
    if not message:
      return 400, {"error": "聊天消息不能为空"}

    if not isFlowerpotQuestion(message):
      return 200, {
          "reply": "我是智能植物养护助手，只处理植物养护、花盆设备和实时环境数据问题。你可以问：现在光照是否合适？需要浇水吗？传感器读数正常吗？"
      }

    messages = [
        {"role": "system", "content": buildSystemPrompt(status["data"])},
        {"role": "user", "content": message},
    ]
    try:
      reply = requestChat(self.settings, messages)
    except RuntimeError:
      return 503, {"error": "AI 服务未配置或暂不可用"}
    return 200, {"reply": reply}


class ApiRequestHandler(BaseHTTPRequestHandler):
  """将 HTTP JSON 请求转发给 Application。"""

  STATIC_DIR = Path(__file__).with_name("static")

  def do_GET(self) -> None:
    """处理 GET 请求。"""
    if not self.path.startswith("/api/"):
      self._serveStaticFile()
      return
    self._handleRequest("GET")

  def do_POST(self) -> None:
    """处理 POST 请求。"""
    self._handleRequest("POST")

  def log_message(self, format: str, *args: object) -> None:
    """关闭默认访问日志，避免在控制台输出请求内容。"""

  def _handleRequest(self, method: str) -> None:
    """解析 JSON 请求体并返回 JSON 响应。

    参数：
      method: 当前请求方法。
    """
    body = None
    if method == "POST":
      body = self._readJsonBody()
      if body is None:
        self._sendJson(400, {"error": "JSON 请求体无效"})
        return

    application = self.server.application
    statusCode, response = application.handle(method, self.path, body)
    self._sendJson(statusCode, response)

  def _readJsonBody(self) -> dict | None:
    """读取并解析 POST 请求中的 JSON 对象。

    返回：
      JSON 字典；无法解析时返回 None。
    """
    try:
      contentLength = int(self.headers.get("Content-Length", "0"))
      rawBody = self.rfile.read(contentLength)
      value = json.loads(rawBody.decode("utf-8"))
    except (UnicodeDecodeError, ValueError):
      return None
    return value if isinstance(value, dict) else None

  def _sendJson(self, statusCode: int, body: dict) -> None:
    """发送 JSON 格式的 HTTP 响应。

    参数：
      statusCode: HTTP 状态码。
      body: 响应字典。
    """
    payload = json.dumps(body, ensure_ascii=False).encode("utf-8")
    self.send_response(statusCode)
    self.send_header("Content-Type", "application/json; charset=utf-8")
    self.send_header("Content-Length", str(len(payload)))
    self.end_headers()
    self.wfile.write(payload)

  def _serveStaticFile(self) -> None:
    """返回手机网页所需的静态文件。"""
    relativePath = "index.html" if self.path == "/" else self.path.lstrip("/")
    requestedPath = (self.STATIC_DIR / relativePath).resolve()
    staticRoot = self.STATIC_DIR.resolve()
    if staticRoot not in requestedPath.parents and requestedPath != staticRoot:
      self.send_error(404)
      return
    if not requestedPath.is_file():
      self.send_error(404)
      return

    contentType = mimetypes.guess_type(requestedPath.name)[0] or "application/octet-stream"
    payload = requestedPath.read_bytes()
    self.send_response(200)
    self.send_header("Content-Type", f"{contentType}; charset=utf-8")
    self.send_header("Content-Length", str(len(payload)))
    self.end_headers()
    self.wfile.write(payload)


def createServer(host: str, port: int, application: Application) -> ThreadingHTTPServer:
  """创建绑定指定地址的智能花盆 HTTP 服务。

  参数：
    host: 监听主机地址。
    port: 监听端口，传入 0 时由系统分配。
    application: API 业务处理对象。

  返回：
    已配置但尚未启动的 HTTP 服务对象。
  """
  server = ThreadingHTTPServer((host, port), ApiRequestHandler)
  server.application = application
  return server


def parseArguments(arguments: list[str] | None = None) -> argparse.Namespace:
  """解析后端的监听地址和端口参数。

  参数：
    arguments: 可选的命令行参数列表；未传入时读取系统参数。

  返回：
    包含 host 与 port 的参数对象。
  """
  parser = argparse.ArgumentParser(description="智能花盆局域网后端")
  parser.add_argument("--host", default="0.0.0.0", help="监听地址")
  parser.add_argument("--port", default=8000, type=int, help="监听端口")
  return parser.parse_args(arguments)


def main() -> None:
  """启动智能花盆局域网 HTTP 服务。"""
  arguments = parseArguments()
  server = createServer(arguments.host, arguments.port, Application())
  print(f"Server running at http://{arguments.host}:{server.server_port}")
  try:
    server.serve_forever()
  except KeyboardInterrupt:
    pass
  finally:
    server.server_close()


if __name__ == "__main__":
  main()
