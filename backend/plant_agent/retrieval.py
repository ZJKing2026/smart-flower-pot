"""本地植物知识库的轻量关键词检索。"""

from dataclasses import dataclass
from pathlib import Path
import re


KNOWLEDGE_ROOT = Path(__file__).parent / "knowledge"


@dataclass(frozen=True)
class KnowledgeHit:
  """表示一条知识库检索结果。"""

  title: str
  content: str
  score: int


def tokenize(text: str) -> set[str]:
  """提取中文连续词、英文单词与数字词。"""
  normalizedText = text.lower()
  chineseTokens = set(re.findall(r"[\u4e00-\u9fff]", normalizedText))
  wordTokens = set(re.findall(r"[a-zA-Z0-9]+", normalizedText))
  return chineseTokens | wordTokens


def searchKnowledge(query: str, limit: int = 3) -> list[KnowledgeHit]:
  """按标题与正文关键词匹配分数检索知识文档。"""
  queryTokens = tokenize(query)
  hits: list[KnowledgeHit] = []
  for path in KNOWLEDGE_ROOT.rglob("*.md"):
    content = path.read_text(encoding="utf-8")
    lines = content.splitlines()
    title = lines[0].lstrip("# ") if lines else path.stem
    score = sum(3 for token in queryTokens if token in title.lower())
    score += sum(1 for token in queryTokens if token in content.lower())
    if score:
      hits.append(KnowledgeHit(title=title, content=content, score=score))
  return sorted(hits, key=lambda hit: hit.score, reverse=True)[:limit]
