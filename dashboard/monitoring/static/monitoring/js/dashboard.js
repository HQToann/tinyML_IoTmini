/**
 * TINYML LANDSLIDE MONITOR - REAL-TIME DASHBOARD JAVASCRIPT
 */

// State variables
let sensorChart = null;
let probChart = null;
let soundEnabled = true;
let audioContext = null;
let isAlertActive = false;

// Audio Siren Synthesizer using Web Audio API
function playAlertSiren() {
    if (!soundEnabled) return;
    try {
        if (!audioContext) {
            audioContext = new (window.AudioContext || window.webkitAudioContext)();
        }
        if (audioContext.state === 'suspended') {
            audioContext.resume();
        }

        const osc = audioContext.createOscillator();
        const gain = audioContext.createGain();

        osc.type = 'sawtooth';
        osc.frequency.setValueAtTime(800, audioContext.currentTime);
        osc.frequency.linearRampToValueAtTime(1600, audioContext.currentTime + 0.15);
        osc.frequency.linearRampToValueAtTime(800, audioContext.currentTime + 0.3);

        gain.gain.setValueAtTime(0.15, audioContext.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.01, audioContext.currentTime + 0.3);

        osc.connect(gain);
        gain.connect(audioContext.destination);

        osc.start();
        osc.stop(audioContext.currentTime + 0.3);
    } catch (e) {
        console.warn("Audio Context error:", e);
    }
}

// Initialize Charts
function initCharts() {
    const commonChartOptions = {
        responsive: true,
        maintainAspectRatio: false,
        animation: { duration: 400 },
        interaction: { intersect: false, mode: 'index' },
        plugins: {
            legend: { display: false },
            tooltip: {
                backgroundColor: 'rgba(15, 23, 42, 0.9)',
                titleColor: '#f8fafc',
                bodyColor: '#cbd5e1',
                borderColor: 'rgba(255, 255, 255, 0.1)',
                borderWidth: 1,
                padding: 10,
                displayColors: true,
            }
        },
        scales: {
            x: {
                grid: { color: 'rgba(255, 255, 255, 0.04)' },
                ticks: { color: '#64748b', font: { size: 10 }, maxRotation: 0 }
            }
        }
    };

    // 1. Sensor Chart (Moisture & Tilt)
    const ctxSensor = document.getElementById('sensorChart').getContext('2d');
    sensorChart = new Chart(ctxSensor, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'Độ ẩm đất (%)',
                    data: [],
                    borderColor: '#38bdf8',
                    backgroundColor: 'rgba(56, 189, 248, 0.1)',
                    borderWidth: 2,
                    tension: 0.3,
                    fill: true,
                    yAxisID: 'yMoisture',
                    pointRadius: 2,
                    pointHoverRadius: 5,
                },
                {
                    label: 'Góc nghiêng (°)',
                    data: [],
                    borderColor: '#fb923c',
                    backgroundColor: 'rgba(251, 146, 60, 0.1)',
                    borderWidth: 2,
                    tension: 0.3,
                    fill: true,
                    yAxisID: 'yTilt',
                    pointRadius: 2,
                    pointHoverRadius: 5,
                }
            ]
        },
        options: {
            ...commonChartOptions,
            scales: {
                ...commonChartOptions.scales,
                yMoisture: {
                    type: 'linear',
                    position: 'left',
                    min: 0,
                    max: 100,
                    grid: { color: 'rgba(255, 255, 255, 0.04)' },
                    ticks: { color: '#38bdf8', font: { size: 10 } }
                },
                yTilt: {
                    type: 'linear',
                    position: 'right',
                    min: 0,
                    max: 90,
                    grid: { drawOnChartArea: false },
                    ticks: { color: '#fb923c', font: { size: 10 } }
                }
            }
        }
    });

    // 2. AI Probability Trend Chart
    const ctxProb = document.getElementById('probChart').getContext('2d');
    probChart = new Chart(ctxProb, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'An toàn (%)',
                    data: [],
                    borderColor: '#10b981',
                    borderWidth: 2,
                    tension: 0.3,
                    pointRadius: 0,
                },
                {
                    label: 'Chú ý (%)',
                    data: [],
                    borderColor: '#f59e0b',
                    borderWidth: 2,
                    tension: 0.3,
                    pointRadius: 0,
                },
                {
                    label: 'Nguy cơ cao (%)',
                    data: [],
                    borderColor: '#ef4444',
                    borderWidth: 2.5,
                    tension: 0.3,
                    pointRadius: 0,
                }
            ]
        },
        options: {
            ...commonChartOptions,
            scales: {
                ...commonChartOptions.scales,
                y: {
                    min: 0,
                    max: 100,
                    grid: { color: 'rgba(255, 255, 255, 0.04)' },
                    ticks: { color: '#94a3b8', font: { size: 10 } }
                }
            }
        }
    });
}

// Fetch & Update Live Telemetry
async function fetchLatestData() {
    try {
        const res = await fetch('/api/latest/');
        if (!res.ok) return;
        const data = await res.json();
        updateUIWithLatest(data);
    } catch (e) {
        console.error("Fetch latest error:", e);
    }
}

// Update DOM elements
function updateUIWithLatest(d) {
    if (!d) return;

    // Node & Broker
    if (d.node_id) document.getElementById('nodeIdText').innerText = d.node_id;

    // Moisture
    const moisture = d.soil_moisture || 0;
    document.getElementById('moistureVal').innerText = moisture.toFixed(1);
    document.getElementById('moistureBar').style.width = Math.min(100, moisture) + '%';
    const moistureState = document.getElementById('moistureState');
    if (moisture < 40) moistureState.innerText = "Trạng thái: Khô ráo (An toàn)";
    else if (moisture < 70) moistureState.innerText = "Trạng thái: Đất ẩm vừa";
    else moistureState.innerText = "Trạng thái: Bão hòa nước (Nguy cơ)";

    // Tilt Angle
    const tilt = d.tilt_angle || 0;
    document.getElementById('tiltAngleVal').innerText = tilt.toFixed(2);
    document.getElementById('tiltAngleBar').style.width = Math.min(100, (tilt / 90) * 100) + '%';
    const tiltState = document.getElementById('tiltAngleState');
    if (tilt < 10) tiltState.innerText = "Địa hình: Ổn định (Dốc phẳng)";
    else if (tilt < 20) tiltState.innerText = "Địa hình: Dốc nghiêng trung bình";
    else tiltState.innerText = "Địa hình: Mái dốc nguy hiểm";

    // Tilt Rate
    const rate = d.tilt_rate || 0;
    document.getElementById('tiltRateVal').innerText = rate.toFixed(3);
    document.getElementById('tiltRateBar').style.width = Math.min(100, (rate / 1.5) * 100) + '%';
    const rateState = document.getElementById('tiltRateState');
    if (rate < 0.08) rateState.innerText = "Chuyển động: Đứng yên";
    else if (rate < 0.4) rateState.innerText = "Chuyển động: Rục rịch nhẹ";
    else rateState.innerText = "Chuyển động: Sụt trượt mạnh!";

    // AI Probabilities
    const pNorm = d.prob_normal || 0;
    const pAtt = d.prob_attention || 0;
    const pDanger = d.prob_high_risk || 0;

    document.getElementById('barSegNormal').style.width = pNorm + '%';
    document.getElementById('barSegAttention').style.width = pAtt + '%';
    document.getElementById('barSegDanger').style.width = pDanger + '%';

    document.getElementById('probNormalVal').innerText = pNorm.toFixed(1) + '%';
    document.getElementById('probNormalLine').style.width = pNorm + '%';

    document.getElementById('probAttentionVal').innerText = pAtt.toFixed(1) + '%';
    document.getElementById('probAttentionLine').style.width = pAtt + '%';

    document.getElementById('probDangerVal').innerText = pDanger.toFixed(1) + '%';
    document.getElementById('probDangerLine').style.width = pDanger + '%';

    // Risk Badge & Banner
    const riskBadge = document.getElementById('riskBadge');
    const riskTitle = document.getElementById('riskTitle');
    const riskDesc = document.getElementById('riskDesc');
    const riskConf = document.getElementById('riskConf');
    const alertBanner = document.getElementById('alertBanner');

    const riskLevel = d.risk_level ?? 0;

    riskBadge.className = 'risk-badge';
    if (riskLevel === 0) {
        riskBadge.classList.add('badge-normal');
        riskTitle.innerText = 'BÌNH THƯỜNG (AN TOÀN)';
        riskDesc.innerText = 'Sườn đồi và độ ẩm đất hoàn toàn trong giới hạn an toàn';
        riskConf.innerText = pNorm.toFixed(0) + '%';
        alertBanner.style.display = 'none';
        isAlertActive = false;
    } else if (riskLevel === 1) {
        riskBadge.classList.add('badge-attention');
        riskTitle.innerText = 'CHÚ Ý (CẢNH BÁO)';
        riskDesc.innerText = 'Phát hiện đất ngấm ẩm hoặc có dấu hiệu chuyển vị mái dốc';
        riskConf.innerText = pAtt.toFixed(0) + '%';
        alertBanner.style.display = 'none';
        isAlertActive = false;
    } else {
        riskBadge.classList.add('badge-danger');
        riskTitle.innerText = 'NGUY CƠ CAO (SẠT LỞ)';
        riskDesc.innerText = 'Đất bão hòa nước và dốc trượt cơ học! Kích hoạt báo động sơ tán khẩn cấp!';
        riskConf.innerText = pDanger.toFixed(0) + '%';
        alertBanner.style.display = 'block';

        if (!isAlertActive) {
            isAlertActive = true;
            playAlertSiren();
        }
    }

    // Last seen
    if (d.last_seen_seconds !== null && d.last_seen_seconds !== undefined) {
        if (d.last_seen_seconds < 5) {
            document.getElementById('lastSeenText').innerText = `Cập nhật: Vừa xong (${d.timestamp})`;
        } else {
            document.getElementById('lastSeenText').innerText = `Cập nhật: ${d.last_seen_seconds}s trước (${d.timestamp})`;
        }
    }
}

// Fetch History & Update Charts and Table
async function fetchHistoryData() {
    try {
        const res = await fetch('/api/history/?limit=30');
        if (!res.ok) return;
        const data = await res.json();

        // Update Sensor Chart
        if (sensorChart && data.timestamps && data.timestamps.length > 0) {
            sensorChart.data.labels = data.timestamps;
            sensorChart.data.datasets[0].data = data.soil_moisture;
            sensorChart.data.datasets[1].data = data.tilt_angle;
            sensorChart.update('none');
        }

        // Update Prob Chart
        if (probChart && data.timestamps && data.timestamps.length > 0) {
            probChart.data.labels = data.timestamps;
            probChart.data.datasets[0].data = data.prob_normal;
            probChart.data.datasets[1].data = data.prob_attention;
            probChart.data.datasets[2].data = data.prob_high_risk;
            probChart.update('none');
        }

        // Update Table
        if (data.raw_records) {
            renderTable(data.raw_records);
        }
    } catch (e) {
        console.error("Fetch history error:", e);
    }
}

// Render Table Rows
function renderTable(records) {
    const tbody = document.getElementById('telemetryTableBody');
    if (!records || records.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" class="empty-row">Chưa có bản ghi telemetry nào trong cơ sở dữ liệu.</td></tr>';
        return;
    }

    let html = '';
    records.forEach(r => {
        let badgeClass = 'tbl-badge-0';
        let badgeText = 'An toàn';
        if (r.risk_level === 1) {
            badgeClass = 'tbl-badge-1';
            badgeText = 'Chú ý';
        } else if (r.risk_level === 2) {
            badgeClass = 'tbl-badge-2';
            badgeText = 'Nguy hiểm';
        }

        html += `
            <tr>
                <td><strong>${r.timestamp}</strong> <span style="color:#64748b;font-size:0.7rem;">(${r.date})</span></td>
                <td><span style="color:#38bdf8;font-weight:600;">${r.soil_moisture}%</span></td>
                <td><span style="color:#fb923c;font-weight:600;">${r.tilt_angle}°</span></td>
                <td>${r.tilt_rate}°/p</td>
                <td><span class="tbl-badge ${badgeClass}">Cấp ${r.risk_level}</span></td>
                <td>${r.status}</td>
                <td><strong>${Math.max(r.prob_normal, r.prob_attention, r.prob_high_risk)}%</strong></td>
            </tr>
        `;
    });
    tbody.innerHTML = html;
}

// Fetch Stats
async function fetchStats() {
    try {
        const res = await fetch('/api/stats/');
        if (!res.ok) return;
        const s = await res.json();

        document.getElementById('totalPacketsCount').innerText = s.total_packets;
        document.getElementById('statNormalCount').innerText = s.normal_count;
        document.getElementById('statAttentionCount').innerText = s.attention_count;
        document.getElementById('statDangerCount').innerText = s.danger_count;
        document.getElementById('statMaxTilt').innerText = s.max_tilt + '°';
        document.getElementById('statAvgMoisture').innerText = s.avg_moisture + '%';
    } catch (e) {
        console.error("Fetch stats error:", e);
    }
}

// Simulation Test Trigger
async function simulatePacket(level) {
    try {
        const res = await fetch('/api/simulate/', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ level: level })
        });
        const result = await res.json();
        if (result.success) {
            fetchLatestData();
            fetchHistoryData();
            fetchStats();
        }
    } catch (e) {
        alert("Lỗi khi gửi gói tin giả lập: " + e);
    }
}

// Clear History Button
document.getElementById('btnClearHistory')?.addEventListener('click', async () => {
    if (confirm("Bạn có chắc chắn muốn xóa toàn bộ lịch sử dữ liệu đo?")) {
        await fetch('/api/clear/', { method: 'POST' });
        fetchLatestData();
        fetchHistoryData();
        fetchStats();
    }
});

// Sound Toggle Button
document.getElementById('soundToggleBtn')?.addEventListener('click', () => {
    soundEnabled = !soundEnabled;
    const btn = document.getElementById('soundToggleBtn');
    const icon = document.getElementById('soundIcon');
    const text = document.getElementById('soundText');

    if (soundEnabled) {
        btn.classList.remove('muted');
        icon.innerText = '🔔';
        text.innerText = 'Còi: BẬT';
    } else {
        btn.classList.add('muted');
        icon.innerText = '🔕';
        text.innerText = 'Còi: TẮT';
    }
});

// Init on Load
document.addEventListener('DOMContentLoaded', () => {
    initCharts();

    // Initial load
    fetchLatestData();
    fetchHistoryData();
    fetchStats();

    // Auto polling intervals
    setInterval(fetchLatestData, 1500);   // Update live cards every 1.5s
    setInterval(fetchHistoryData, 3000);  // Update charts every 3s
    setInterval(fetchStats, 5000);        // Update stats every 5s
});
