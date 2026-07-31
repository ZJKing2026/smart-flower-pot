"""后端配置读取模块。"""

import os
from pathlib import Path


def loadEnvFile(envPath: Path) -> None:
  """读取本地环境变量文件，不覆盖已有系统环境变量。

  参数：
    envPath: 环境变量文件路径。
  """
  if not envPath.exists():
    return

  for line in envPath.read_text(encoding="utf-8").splitlines():
    if not line or line.lstrip().startswith("#") or "=" not in line:
      continue

    key, value = line.split("=", 1)
    os.environ.setdefault(key.strip(), value.strip())


def getSettings() -> dict[str, str]:
  """返回后端运行所需的非敏感配置结构。

  返回：
    包含 DeepSeek 接口地址、模型名和密钥的配置字典。
  """
  loadEnvFile(Path(__file__).with_name(".env"))
  return {
      "deepseekApiKey": os.getenv("DEEPSEEK_API_KEY", ""),
      "deepseekBaseUrl": os.getenv("DEEPSEEK_BASE_URL", "https://api.deepseek.com"),
      "deepseekModel": os.getenv("DEEPSEEK_MODEL", "deepseek-chat"),
  }
