# 植物养护 Agent 平台实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建独立植物养护 Agent API、可审核的本地 RAG 知识库和植物工作台 UI，并复用现有智能花盆状态接口。

**Architecture:** 新平台位于 `backend/plant_agent/`，与现有花盆后端隔离。RAG 使用 Markdown、标签和中文关键词评分；Agent 根据意图检索知识、附带可选设备状态后调用 DeepSeek，流式接口与缓存由后端统一负责。

**Tech Stack:** Python 标准库、DeepSeek OpenAI 兼容 API、Markdown 文档、HTML/CSS/JavaScript、unittest。

---

### Task 1: 建立知识库与检索模块

**Files:**
- Create: `backend/plant_agent/knowledge/plants/succulent.md`
- Create: `backend/plant_agent/knowledge/plants/indoor-plants.md`
- Create: `backend/plant_agent/knowledge/devices/smart-flower-pot.md`
- Create: `backend/plant_agent/retrieval.py`
- Create: `backend/plant_agent/tests/test_retrieval.py`

- [ ] 先写检索测试：查询“我想买个多肉”必须返回 `succulent.md`；查询“土壤 ADC 4095”必须返回设备文档。
- [ ] 实现 Markdown 标题、标签和正文读取；按查询词与标题/标签/正文的加权次数排序，返回前三个片段与文档标题。
- [ ] 运行 `python -m unittest backend.plant_agent.tests.test_retrieval -v`，确认检索测试通过。

### Task 2: 建立意图路由、缓存和 Agent API

**Files:**
- Create: `backend/plant_agent/router.py`
- Create: `backend/plant_agent/cache.py`
- Create: `backend/plant_agent/app.py`
- Create: `backend/plant_agent/tests/test_router.py`
- Create: `backend/plant_agent/tests/test_app.py`

- [ ] 先写分类测试：多肉购买为 `plant_recommendation`；换盆为 `daily_care`；病斑为 `disease_check`；GPIO/继电器为 `device_diagnosis`；Python 教程为 `out_of_scope`。
- [ ] 实现本地意图路由：仅明确无关内容进入 `out_of_scope`，其余植物自然表达放行。
- [ ] 实现 60 秒问答缓存，缓存键由规范化问题与设备状态 JSON 组成。
- [ ] 实现 `POST /api/agent/chat`，返回 `intent`、`answer`、`sources`、`cached`；无关问题不调用 DeepSeek。
- [ ] 运行对应单元测试，确认无关问题不检索/不调用模型，相关问题携带 RAG 片段。

### Task 3: 接入现有花盆状态与结构化分析

**Files:**
- Modify: `backend/plant_agent/app.py`
- Create: `backend/plant_agent/tests/test_device_analysis.py`

- [ ] 定义花盆状态适配器，读取既有 `DeviceStore` 的最新状态，不改变 `/api/device/*` 协议。
- [ ] 实现 `POST /api/agent/analyze-device`，要求模型按“数据解读、风险判断、建议操作、注意事项”返回纯文本分段。
- [ ] 测试离线状态、ADC=4095 未标定提示，以及回答不包含自动开泵指令。

### Task 4: 构建植物工作台 UI

**Files:**
- Create: `backend/plant_agent/static/index.html`
- Create: `backend/plant_agent/static/style.css`
- Create: `backend/plant_agent/static/app.js`
- Create: `backend/plant_agent/tests/test_static_security.py`

- [ ] 创建桌面三栏工作台：导航、对话/知识内容、智能花盆状态；移动端单列。
- [ ] 使用深绿、米白与浅叶绿设计令牌，回答渲染为标题、段落和列表卡片；禁止用 `innerHTML` 渲染模型文字。
- [ ] 实现流式显示占位与逐行追加；在不支持流式的后端阶段保持普通请求回退。
- [ ] 添加静态安全测试，保证模型返回内容通过 `textContent` 渲染，且手机宽度下无横向溢出。

### Task 5: 集成验证与说明

**Files:**
- Create: `backend/plant_agent/README.md`
- Modify: `docs/project-progress.md`

- [ ] 运行 `python -m unittest discover -s backend\plant_agent\tests -v` 与现有 `backend\ai_assistant` 全量测试。
- [ ] 启动 Agent 服务，验证多肉选购、花盆实时分析、无关问题、缓存命中和移动端页面。
- [ ] 记录启动命令、API 示例、RAG 文档增加方式与安全边界；不写入任何 API Key 或 Wi-Fi 密码。
