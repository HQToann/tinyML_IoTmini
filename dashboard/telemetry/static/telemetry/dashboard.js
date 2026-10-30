const labels = {normal: "Bình thường", attention: "Chú ý", high_risk_in_model: "Nguy cơ cao trong mô hình"};
function n(value, digits = 1) {
  const number = Number(value);
  return Number.isFinite(number) ? number.toLocaleString("vi-VN", {maximumFractionDigits: digits}) : "—";
}
function dt(value) {
  return value ? new Intl.DateTimeFormat("vi-VN", {dateStyle: "short", timeStyle: "medium"}).format(new Date(value)) : "—";
}
function ago(value) {
  const seconds = Math.max(0, Math.floor((Date.now() - new Date(value).getTime()) / 1000));
  return seconds < 5 ? "Vừa nhận" : seconds < 60 ? `${seconds} giây trước` : `${Math.floor(seconds / 60)} phút trước`;
}
function esc(value) {
  return String(value).replace(/[&<>"']/g, char => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"}[char]));
}
function showError(message) {
  const element = document.getElementById("error");
  element.textContent = message;
  element.classList.add("show");
  setTimeout(() => element.classList.remove("show"), 4500);
}
function render(reading, total) {
  const stale = Date.now() - new Date(reading.created_at).getTime() > 15000;
  document.getElementById("banner").className = `banner status-${reading.status}${stale ? " stale" : ""}`;
  document.getElementById("symbol").textContent = {normal: "✓", attention: "!", high_risk_in_model: "‼"}[reading.status] || "…";
  document.getElementById("state").textContent = labels[reading.status] || reading.status_label || "Không rõ";
  document.getElementById("detail").textContent = stale ? "Thiết bị chưa gửi dữ liệu mới hơn 15 giây." : "Trạng thái phân loại gần nhất từ model trên ESP32.";
  document.getElementById("age").textContent = ago(reading.created_at);
  document.getElementById("angle").textContent = n(reading.tilt_angle_deg, 2);
  document.getElementById("rate").textContent = n(reading.tilt_rate_deg_per_min, 2);
  document.getElementById("device").textContent = reading.device_id;
  document.getElementById("device2").textContent = reading.device_id;
  document.getElementById("lastseen").textContent = ago(reading.created_at);
  document.getElementById("received").textContent = dt(reading.created_at);
  const badge = document.getElementById("badge");
  badge.textContent = stale ? "Ngoại tuyến" : "Đang gửi";
  badge.className = stale ? "pill offline" : "pill online";
  document.getElementById("count").textContent = `${total} bản tin gần nhất`;
}
function draw(items) {
  const canvas = document.getElementById("chart"), rect = canvas.getBoundingClientRect();
  if (!rect.width || !rect.height) return;
  const ratio = devicePixelRatio || 1;
  canvas.width = rect.width * ratio; canvas.height = rect.height * ratio;
  const ctx = canvas.getContext("2d"); ctx.scale(ratio, ratio);
  const data = [...items].reverse();
  const series = [["tilt_angle_deg", "GÓC NGHIÊNG (°)", "#ffbd4a"], ["tilt_rate_deg_per_min", "TỐC ĐỘ ĐỔI GÓC (°/PHÚT)", "#ac91ff"]];
  series.forEach(([key, title, color], index) => {
    const h = rect.height / 2, top = index * h, yTop = top + 20, yBottom = top + h - 12, left = 48, right = rect.width - 6;
    const values = data.map(item => Number(item[key])).filter(Number.isFinite);
    ctx.fillStyle = "#8294ac"; ctx.font = "10px system-ui"; ctx.fillText(title, 2, top + 13);
    ctx.strokeStyle = "#26364b"; ctx.beginPath(); ctx.moveTo(left, yBottom); ctx.lineTo(right, yBottom); ctx.stroke();
    if (!values.length) return;
    let min = Math.min(...values), max = Math.max(...values); if (min === max) { min--; max++; }
    const pad = (max - min) * 0.12; min -= pad; max += pad;
    ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.beginPath();
    values.forEach((value, i) => {
      const x = left + (values.length < 2 ? 0 : (right - left) * i / (values.length - 1));
      const y = yBottom - (value - min) / (max - min) * (yBottom - yTop);
      i ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
    });
    ctx.stroke(); ctx.fillStyle = "#7588a0"; ctx.fillText(n(max), 2, yTop + 4); ctx.fillText(n(min), 2, yBottom);
  });
}
function renderTable(items) {
  const body = document.getElementById("rows");
  if (!items.length) { body.innerHTML = '<tr><td colspan="5" class="emptyrow">Chưa có dữ liệu từ ESP32.</td></tr>'; return; }
  body.innerHTML = items.slice(0, 20).map(r =>
    `<tr><td>${dt(r.created_at)}</td><td>${esc(r.device_id)}</td><td>${n(r.tilt_angle_deg, 2)}°</td>` +
    `<td>${n(r.tilt_rate_deg_per_min, 2)}°/phút</td><td><span class="tag tag-${r.status}">${esc(labels[r.status] || r.status_label)}</span></td></tr>`
  ).join("");
}
async function refresh() {
  try {
    const response = await fetch("/api/readings/?limit=60", {cache: "no-store"});
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const items = (await response.json()).items || [];
    document.getElementById("dot").className = items.length && Date.now() - new Date(items[0].created_at).getTime() < 15000 ? "online" : "";
    document.getElementById("connection").textContent = "Dashboard hoạt động";
    renderTable(items); draw(items); if (items.length) render(items[0], items.length);
  } catch (error) {
    document.getElementById("connection").textContent = "Mất kết nối";
    showError(`Không tải được dữ liệu: ${error.message}`);
  }
}
refresh();
setInterval(refresh, 1000);
window.addEventListener("resize", refresh);
