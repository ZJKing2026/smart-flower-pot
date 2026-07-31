"""植物养护 Agent 的短时内存缓存。"""

import json
import time


class AnswerCache:
  """保存短时问答结果，减少重复模型调用。"""

  def __init__(self, ttlSeconds: int = 60) -> None:
    """初始化缓存有效期。"""
    self.ttlSeconds = ttlSeconds
    self.entries: dict[str, tuple[float, dict]] = {}

  def buildKey(self, question: str, deviceContext: dict | None) -> str:
    """根据问题和设备状态生成稳定缓存键。"""
    return json.dumps({"question": question.strip().lower(), "device": deviceContext}, ensure_ascii=False, sort_keys=True)

  def get(self, key: str) -> dict | None:
    """读取未过期的缓存结果。"""
    entry = self.entries.get(key)
    if entry is None or time.monotonic() - entry[0] > self.ttlSeconds:
      self.entries.pop(key, None)
      return None
    return entry[1]

  def set(self, key: str, value: dict) -> None:
    """保存一条缓存结果。"""
    self.entries[key] = (time.monotonic(), value)
