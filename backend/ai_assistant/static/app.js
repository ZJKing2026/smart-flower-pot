const metricLabels = [
  ["temperature", "温度", "°C"],
  ["humidity", "湿度", "%"],
  ["light", "光照", "lx"],
  ["soilAdc", "土壤 ADC", ""],
  ["alert", "告警", ""],
  ["pump", "水泵", ""],
];

async function requestJson(url, options = {}) {
  const response = await fetch(url, options);
  const body = await response.json();
  if (!response.ok) {
    throw new Error(body.error || "请求失败");
  }
  return body;
}

function renderMetrics(data) {
  const metrics = document.querySelector("#metrics");
  metrics.replaceChildren();
  metricLabels.forEach(([key, label, unit]) => {
    const value = data[key] ?? "--";
    const item = document.createElement("div");
    const title = document.createElement("span");
    const content = document.createElement("strong");

    item.className = "metric";
    title.textContent = label;
    content.textContent = `${value} ${unit}`.trim();
    item.append(title, content);
    metrics.append(item);
  });
}

async function refreshStatus() {
  try {
    const status = await requestJson("/api/device/status");
    document.querySelector("#deviceState").textContent = status.online ? "设备在线" : "设备离线";
    document.querySelector("#updatedAt").textContent = status.updatedAt
      ? `更新时间：${new Date(status.updatedAt * 1000).toLocaleString()}`
      : "暂无设备数据";
    renderMetrics(status.data || {});
  } catch (error) {
    document.querySelector("#deviceState").textContent = "后端不可用";
  }
}

async function requestAdvice() {
  const output = document.querySelector("#adviceText");
  output.textContent = "正在分析…";
  try {
    const body = await requestJson("/api/ai/advice", { method: "POST", headers: { "Content-Type": "application/json" }, body: "{}" });
    output.textContent = body.advice;
  } catch (error) {
    output.textContent = error.message;
  }
}

async function sendChat() {
  const input = document.querySelector("#chatInput");
  const log = document.querySelector("#chatLog");
  const message = input.value.trim();
  if (!message) return;
  log.textContent = "正在询问…";
  try {
    const body = await requestJson("/api/ai/chat", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ message }) });
    log.textContent = `你：${message}\n\n花盆助手：${body.reply}`;
    input.value = "";
  } catch (error) {
    log.textContent = error.message;
  }
}

async function sendCommand(action) {
  const output = document.querySelector("#commandResult");
  try {
    const body = await requestJson("/api/device/command", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ action }) });
    output.textContent = body.message;
  } catch (error) {
    output.textContent = error.message;
  }
}

document.querySelector("#adviceButton").addEventListener("click", requestAdvice);
document.querySelector("#chatButton").addEventListener("click", sendChat);
document.querySelector("#waterButton").addEventListener("click", () => sendCommand("water"));
document.querySelector("#stopButton").addEventListener("click", () => sendCommand("stop"));

refreshStatus();
setInterval(refreshStatus, 3000);
