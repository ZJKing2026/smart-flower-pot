# 专业花盆助手实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将后端聊天限制为智能植物养护与设备诊断，并输出更完整的环境分析报告。

**Architecture:** 在 DeepSeek 请求前加入纯本地主题判定函数；无关消息直接返回固定范围说明。相关消息仍附带最新设备数据，但系统提示词改为专业养护角色；环境分析接口使用四段式报告要求。

**Tech Stack:** Python 3 标准库、unittest、现有 DeepSeek Chat Completions 客户端。

---

### Task 1: 为主题过滤补充失败测试

**Files:**
- Modify: `backend/ai_assistant/tests/test_ai_client.py`
- Modify: `backend/ai_assistant/tests/test_app.py`

- [ ] **Step 1: 添加主题判定测试**

```python
def testIsFlowerpotQuestionAcceptsPlantQuestion(self):
  self.assertTrue(isFlowerpotQuestion("现在光照太低怎么办？"))

def testIsFlowerpotQuestionRejectsProgrammingQuestion(self):
  self.assertFalse(isFlowerpotQuestion("我想学习 Python 基础知识"))
```

- [ ] **Step 2: 添加无关问题不调用模型的接口测试**

```python
def testChatRejectsUnrelatedQuestionWithoutCallingModel(self):
  with patch("backend.ai_assistant.app.requestChat") as requestChat:
    status, body = self.application.handle(
        "POST", "/api/ai/chat", {"message": "我想学习 Python 基础知识"}
    )
  self.assertEqual(status, 200)
  self.assertIn("只处理", body["reply"])
  requestChat.assert_not_called()
```

- [ ] **Step 3: 运行测试确认失败**

Run:

```powershell
python -m unittest backend.ai_assistant.tests.test_ai_client backend.ai_assistant.tests.test_app -v
```

Expected: 因 `isFlowerpotQuestion` 尚不存在或无关问题仍调用模型而失败。

### Task 2: 实现本地主题过滤与专业提示词

**Files:**
- Modify: `backend/ai_assistant/ai_client.py`
- Modify: `backend/ai_assistant/app.py`

- [ ] **Step 1: 实现主题判定函数**

在 `ai_client.py` 定义关键词集合，覆盖植物、养护、浇水、土壤、光照、温湿度、传感器、花盆、ESP32、OLED、蜂鸣器、继电器、水泵、告警；函数将消息转小写后检查关键词。

```python
def isFlowerpotQuestion(message: str) -> bool:
  """判断消息是否属于花盆养护或设备诊断主题。"""
  keywords = ("植物", "花", "浇水", "土壤", "光照", "温度", "湿度", "传感器", "花盆", "水泵", "继电器", "蜂鸣器", "告警", "esp32", "oled")
  normalizedMessage = message.lower()
  return any(keyword in normalizedMessage for keyword in keywords)
```

- [ ] **Step 2: 替换系统提示词**

删除“建议必须简短”，改为要求基于设备数据、分点说明判断依据和建议操作；明确土壤 ADC 未标定、自动浇水关闭、AI 不可直接控制水泵。

- [ ] **Step 3: 在聊天入口拦截无关消息**

在 `_handleChat()` 校验消息非空后、调用 `requestChat()` 前加入：

```python
if not isFlowerpotQuestion(message):
  return 200, {
      "reply": "我是智能植物养护助手，只处理植物养护、花盆设备和实时环境数据问题。你可以问：现在光照是否合适？需要浇水吗？传感器读数正常吗？"
  }
```

- [ ] **Step 4: 使环境分析采用四段式请求**

将 `_handleAdvice()` 的用户请求替换为：

```python
"请按“数据解读、风险判断、建议操作、注意事项”四个部分分析当前环境。"
```

- [ ] **Step 5: 运行新增测试确认通过**

Run:

```powershell
python -m unittest backend.ai_assistant.tests.test_ai_client backend.ai_assistant.tests.test_app -v
```

Expected: 主题判定、无关拒答、相关聊天与环境分析测试全部通过。

### Task 3: 全量验证与人工验证

**Files:**
- Modify: `docs/project-progress.md`

- [ ] **Step 1: 运行全部后端测试**

Run:

```powershell
python -m unittest discover -s backend\ai_assistant\tests -v
```

Expected: 既有 11 项与新增测试全部通过。

- [ ] **Step 2: 启动后端并重启后端进程**

Run:

```powershell
python -m backend.ai_assistant.app --host 0.0.0.0 --port 8000
```

Expected: 手机页面刷新后使用新过滤规则；不改动 `.env` 内容。

- [ ] **Step 3: 人工验证三类输入**

在手机页面依次发送“我想学习 Python 基础知识”“现在需要浇水吗？”并点击“分析当前环境”。

Expected: 第一项立即返回范围说明；第二项为花盆相关回答；环境分析含四个明确部分。

- [ ] **Step 4: 记录进度**

在 `docs/project-progress.md` 追加：过滤规则、结构化环境分析、全量测试结果与手机实测结果；不记录 API Key。
