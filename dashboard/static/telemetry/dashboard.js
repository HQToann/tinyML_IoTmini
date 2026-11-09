/* ==========================================================================
   TINYML HOUSE TILT MONITOR - JAVASCRIPT (CHUYÊN BIỆT GÓC & VẬN TỐC GÓC)
   Handles: Real-time Telemetry, Charts (Chart.js), Simulation, Audio Alerts,
   Tare / Zero Angle Calibration, and Clean Minimalist Toast Notifications.
   Không sử dụng cảm biến độ ẩm. Zero icons, zero emojis.
   ========================================================================== */

(function () {
    "use strict";

    // --- State Variables ---
    let soundEnabled = true;
    let sensorChartInstance = null;
    let probChartInstance = null;
    let cachedReadings = [];
    let pollingTimer = null;
    let lastAlertAudioTime = 0;

    // Tare / Zero Angle Calibration State
    let zeroAngleOffset = parseFloat(localStorage.getItem("dashboard_zero_angle_offset")) || 0.0;
    let isCalibrated = localStorage.getItem("dashboard_is_calibrated") === "true";
    let latestRawAngle = 0.0;

    // Web Audio Context for alarm sound
    let audioCtx = null;

    function initAudio() {
        if (!audioCtx) {
            const AudioContext = window.AudioContext || window.webkitAudioContext;
            if (AudioContext) {
                audioCtx = new AudioContext();
            }
        }
    }

    function playBuzzerAlert() {
        if (!soundEnabled) return;
        const now = Date.now();
        if (now - lastAlertAudioTime < 2500) return; // Debounce audio beeps
        lastAlertAudioTime = now;

        try {
            initAudio();
            if (!audioCtx) return;
            if (audioCtx.state === "suspended") {
                audioCtx.resume();
            }

            const osc = audioCtx.createOscillator();
            const gain = audioCtx.createGain();

            osc.type = "sawtooth";
            osc.frequency.setValueAtTime(880, audioCtx.currentTime); // A5
            osc.frequency.exponentialRampToValueAtTime(440, audioCtx.currentTime + 0.35);

            gain.gain.setValueAtTime(0.85, audioCtx.currentTime);
            gain.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + 0.35);

            osc.connect(gain);
            gain.connect(audioCtx.destination);

            osc.start();
            osc.stop(audioCtx.currentTime + 0.35);
        } catch (e) {
            console.warn("Audio playback not allowed yet:", e);
        }
    }

    // --- Toast Notifications ---
    function showToast(message, type = "info") {
        let container = document.querySelector(".toast-container");
        if (!container) {
            container = document.createElement("div");
            container.className = "toast-container";
            document.body.appendChild(container);
        }

        const toast = document.createElement("div");
        toast.className = `toast-msg toast-${type}`;
        toast.innerHTML = `<span class="toast-indicator"></span><span>${message}</span>`;
        container.appendChild(toast);

        setTimeout(() => {
            toast.style.opacity = "0";
            toast.style.transform = "translateY(10px)";
            toast.style.transition = "all 0.25s ease";
            setTimeout(() => toast.remove(), 250);
        }, 3200);
    }

    // --- Format Helpers ---
    function formatNum(val, decimals = 2) {
        if (val === null || val === undefined || isNaN(val)) return "0.00";
        return Number(val).toFixed(decimals);
    }

    function formatTime(isoString) {
        if (!isoString) return "—";
        const d = new Date(isoString);
        return d.toLocaleTimeString("vi-VN", { hour12: false });
    }

    function formatDateTime(isoString) {
        if (!isoString) return "—";
        const d = new Date(isoString);
        return d.toLocaleString("vi-VN", {
            hour: "2-digit",
            minute: "2-digit",
            second: "2-digit",
            day: "2-digit",
            month: "2-digit"
        });
    }

    // --- Tính toán phân bố xác suất Softmax dựa trên Góc & Tốc độ đổi góc ---
    function calculateProbabilities(reading) {
        const status = reading.status || "normal";
        const tilt = Math.abs(Number(reading.tilt_angle_deg) || 0);
        const rate = Math.abs(Number(reading.tilt_rate_deg_per_min) || 0);

        let pNormal, pAttention, pDanger;

        if (status === "high_risk_in_model" || status === "danger" || status === 2) {
            pDanger = Math.min(99.5, Math.max(75.0, 75 + (tilt * 2.5) + (rate * 12)));
            pAttention = Math.max(0.5, (100 - pDanger) * 0.8);
            pNormal = Math.max(0.0, 100 - pDanger - pAttention);
        } else if (status === "attention" || status === 1) {
            pAttention = Math.min(92.0, Math.max(68.0, 70 + (tilt * 3.5) + (rate * 8)));
            pDanger = Math.max(1.0, (100 - pAttention) * 0.25);
            pNormal = Math.max(0.0, 100 - pAttention - pDanger);
        } else {
            pNormal = Math.min(99.8, Math.max(88.0, 98 - (tilt * 1.5) - (rate * 5)));
            pAttention = Math.max(0.2, (100 - pNormal) * 0.8);
            pDanger = Math.max(0.0, 100 - pNormal - pAttention);
        }

        const total = pNormal + pAttention + pDanger;
        return {
            normal: ((pNormal / total) * 100),
            attention: ((pAttention / total) * 100),
            danger: ((pDanger / total) * 100)
        };
    }

    // --- Khởi tạo Chart.js ---
    function initCharts() {
        const sensorCtx = document.getElementById("sensorChart");
        const probCtx = document.getElementById("probChart");
        if (!sensorCtx || !probCtx) return;

        const textColor = "#64748b";
        const gridColor = "#f1f5f9";

        const commonScales = {
            x: {
                grid: { color: gridColor },
                ticks: { color: textColor, font: { family: "JetBrains Mono", size: 10 } }
            },
            y: {
                grid: { color: gridColor },
                ticks: { color: textColor, font: { family: "JetBrains Mono", size: 10 } }
            }
        };

        // 1. Biểu đồ góc nghiêng kết cấu
        sensorChartInstance = new Chart(sensorCtx, {
            type: "line",
            data: {
                labels: [],
                datasets: [
                    {
                        label: "Góc nghiêng (°)",
                        data: [],
                        borderColor: "#ea580c",
                        backgroundColor: "rgba(234, 88, 12, 0.08)",
                        borderWidth: 2,
                        tension: 0.25,
                        fill: true,
                        pointRadius: 2,
                        pointHoverRadius: 4
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: { duration: 300 },
                plugins: {
                    legend: { display: false },
                    tooltip: { mode: "index", intersect: false }
                },
                scales: {
                    x: commonScales.x,
                    y: {
                        min: 0,
                        suggestedMax: 10,
                        grid: { color: gridColor },
                        ticks: {
                            color: textColor,
                            font: { family: "JetBrains Mono", size: 10 },
                            callback: (v) => v + "°"
                        }
                    }
                }
            }
        });

        // 2. Biểu đồ xu hướng xác suất TinyML
        probChartInstance = new Chart(probCtx, {
            type: "line",
            data: {
                labels: [],
                datasets: [
                    {
                        label: "An toàn (%)",
                        data: [],
                        borderColor: "#16a34a",
                        backgroundColor: "rgba(22, 163, 74, 0.06)",
                        borderWidth: 1.8,
                        tension: 0.25,
                        fill: false,
                        pointRadius: 2,
                        pointHoverRadius: 4
                    },
                    {
                        label: "Chú ý (%)",
                        data: [],
                        borderColor: "#d97706",
                        backgroundColor: "rgba(217, 119, 6, 0.06)",
                        borderWidth: 1.8,
                        tension: 0.25,
                        fill: false,
                        pointRadius: 2,
                        pointHoverRadius: 4
                    },
                    {
                        label: "Nguy hiểm (%)",
                        data: [],
                        borderColor: "#dc2626",
                        backgroundColor: "rgba(220, 38, 38, 0.08)",
                        borderWidth: 2,
                        tension: 0.25,
                        fill: false,
                        pointRadius: 2,
                        pointHoverRadius: 4
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: { duration: 300 },
                plugins: {
                    legend: { display: false },
                    tooltip: { mode: "index", intersect: false }
                },
                scales: {
                    x: commonScales.x,
                    y: {
                        min: 0,
                        max: 100,
                        grid: { color: gridColor },
                        ticks: {
                            color: textColor,
                            font: { family: "JetBrains Mono", size: 10 },
                            callback: (v) => v + "%"
                        }
                    }
                }
            }
        });
    }

    // --- UI Update Pipeline ---
    function updateDashboard(readings) {
        if (!readings || !readings.length) {
            const tableBody = document.getElementById("telemetryTableBody");
            if (tableBody && !tableBody.hasChildNodes()) {
                tableBody.innerHTML = `<tr><td colspan="5" class="cell-empty">Đang chờ bản tin từ thiết bị...</td></tr>`;
            }
            return;
        }

        const latest = readings[0];
        const rawTiltAngle = Math.abs(Number(latest.tilt_angle_deg) || 0);
        latestRawAngle = rawTiltAngle;

        const isDeviceUncalibrated = (latest.status === "uncalibrated" || latest.is_calibrated === false) && !isCalibrated;

        // Effective tilt angle after Tare/Zero Calibration
        const effectiveTiltAngle = isCalibrated
            ? Math.max(0, Math.abs(rawTiltAngle - zeroAngleOffset))
            : rawTiltAngle;

        // Update hint in control box
        const calibHint = document.getElementById("calibCurrentAngleHint");
        if (calibHint) {
            calibHint.textContent = `Góc hiện tại: ${formatNum(rawTiltAngle, 2)}°`;
        }

        // Update calibration indicator & reset buttons
        const calibIndicatorText = document.getElementById("calibIndicatorText");
        const calibStatusInfo = document.getElementById("calibStatusInfo");
        const calibStatusText = document.getElementById("calibStatusText");
        const btnResetQuick = document.getElementById("btnResetQuick");
        const btnResetCalib = document.getElementById("btnResetCalib");

        if (isCalibrated) {
            if (calibIndicatorText) {
                calibIndicatorText.textContent = `Mốc: -${formatNum(zeroAngleOffset, 2)}°`;
                calibIndicatorText.title = `Góc gốc: ${formatNum(rawTiltAngle, 2)}° | Mốc 0°: ${formatNum(zeroAngleOffset, 2)}°`;
            }
            if (btnResetQuick) btnResetQuick.style.display = "inline-flex";
            if (btnResetCalib) btnResetCalib.style.display = "inline-block";
            if (calibStatusInfo && calibStatusText) {
                calibStatusInfo.style.display = "inline-flex";
                calibStatusText.innerHTML = `Đang bù mốc: <strong>${formatNum(effectiveTiltAngle, 2)}°</strong> (Lệch: ${zeroAngleOffset >= 0 ? "+" : ""}${formatNum(zeroAngleOffset, 2)}°)`;
            }
        } else {
            if (calibIndicatorText) {
                calibIndicatorText.textContent = isDeviceUncalibrated ? "Mốc: Chưa cài" : "Mốc: Gốc";
                calibIndicatorText.title = isDeviceUncalibrated ? "Chưa bấm nút cân bằng mốc 0°" : "Đang lấy trực tiếp góc cảm biến";
            }
            if (btnResetQuick) btnResetQuick.style.display = "none";
            if (btnResetCalib) btnResetCalib.style.display = "none";
            if (calibStatusInfo) calibStatusInfo.style.display = "none";
        }

        // Effective reading for AI classification
        let computedStatus = latest.status || "normal";
        if (isDeviceUncalibrated) {
            computedStatus = "uncalibrated";
        } else if (isCalibrated) {
            if (effectiveTiltAngle < 2.5) {
                computedStatus = "normal";
            } else if (effectiveTiltAngle >= 5.0) {
                computedStatus = "high_risk_in_model";
            } else if (effectiveTiltAngle >= 2.5) {
                computedStatus = "attention";
            }
        }

        const effectiveReading = {
            ...latest,
            tilt_angle_deg: effectiveTiltAngle,
            status: computedStatus
        };

        const probs = calculateProbabilities(effectiveReading);
        const isDanger = computedStatus === "high_risk_in_model" || computedStatus === "danger" || computedStatus === 2;
        const isAttention = computedStatus === "attention" || computedStatus === 1;

        // 1. Alert Banner (Never alert when uncalibrated)
        const alertBanner = document.getElementById("alertBanner");
        if (isDanger && !isDeviceUncalibrated) {
            if (alertBanner) alertBanner.style.display = "block";
            playBuzzerAlert();
        } else {
            if (alertBanner) alertBanner.style.display = "none";
        }

        // 2. Card 1: AI Assessment
        const riskBadge = document.getElementById("riskBadge");
        const riskTitle = document.getElementById("riskTitle");
        const riskDesc = document.getElementById("riskDesc");
        const riskConf = document.getElementById("riskConf");
        const lastSeenText = document.getElementById("lastSeenText");

        if (riskBadge && riskTitle && riskDesc && riskConf) {
            if (isDeviceUncalibrated) {
                riskBadge.className = "ai-badge badge-uncalibrated";
                riskTitle.textContent = "Chờ hiệu chuẩn mốc 0°";
                riskDesc.textContent = "Chưa kích hoạt đo & cảnh báo. Bấm 'Cân bằng 0°' để bắt đầu giám sát.";
                riskConf.textContent = "—";
            } else {
                riskBadge.className = "ai-badge " + (isDanger ? "badge-danger" : isAttention ? "badge-attention" : "badge-normal");
                riskTitle.textContent = isDanger ? "Nguy cơ cao (Sập đổ)" : isAttention ? "Chú ý cảnh báo" : "Bình thường";
                riskDesc.textContent = isDanger
                    ? "Kết cấu nghiêng nghiêm trọng vượt ngưỡng an toàn. Yêu cầu kiểm tra hoặc sơ tán lập tức."
                    : isAttention
                    ? "Phát hiện biến thiên góc hoặc dấu hiệu lún bất thường, cần theo dõi sát."
                    : (isCalibrated ? "Góc nghiêng kết cấu ở ngưỡng an toàn (đã cân bằng mốc 0°)." : "Góc nghiêng kết cấu ngôi nhà ở ngưỡng an toàn.");
                const maxProb = Math.max(probs.normal, probs.attention, probs.danger);
                riskConf.textContent = formatNum(maxProb, 1) + "%";
            }
        }
        if (lastSeenText) {
            lastSeenText.textContent = "Cập nhật " + formatTime(latest.created_at);
        }

        // 3. Card 2: Tilt Angle
        const tiltAngleVal = document.getElementById("tiltAngleVal");
        const tiltAngleBar = document.getElementById("tiltAngleBar");
        const tiltAngleState = document.getElementById("tiltAngleState");

        if (tiltAngleVal) {
            tiltAngleVal.textContent = isDeviceUncalibrated ? "—" : formatNum(effectiveTiltAngle, 2);
        }
        if (tiltAngleBar) {
            const pct = isDeviceUncalibrated ? 0 : Math.min(100, (effectiveTiltAngle / 15) * 100);
            tiltAngleBar.style.width = pct + "%";
        }
        if (tiltAngleState) {
            tiltAngleState.textContent = isDeviceUncalibrated
                ? `Góc thô: ${formatNum(rawTiltAngle, 2)}° (chưa chốt mốc)`
                : effectiveTiltAngle > 5.0
                ? "Nghiêng nghiêm trọng (>5.0°)"
                : effectiveTiltAngle > 2.5
                ? "Bắt đầu nghiêng (2.5° - 5.0°)"
                : (isCalibrated ? "Kết cấu ổn định (đã cân bằng)" : "Kết cấu ổn định (<2.5°)");
        }

        // 4. Card 3: Tilt Rate
        const tiltRateVal = document.getElementById("tiltRateVal");
        const tiltRateBar = document.getElementById("tiltRateBar");
        const tiltRateState = document.getElementById("tiltRateState");
        const tiltRate = Math.abs(Number(latest.tilt_rate_deg_per_min) || 0);

        if (tiltRateVal) {
            tiltRateVal.textContent = isDeviceUncalibrated ? "—" : formatNum(tiltRate, 3);
        }
        if (tiltRateBar) {
            const pct = isDeviceUncalibrated ? 0 : Math.min(100, (tiltRate / 0.8) * 100);
            tiltRateBar.style.width = pct + "%";
        }
        if (tiltRateState) {
            tiltRateState.textContent = isDeviceUncalibrated
                ? "Chưa kích hoạt"
                : tiltRate > 0.4
                ? "Dịch chuyển nhanh (>0.4°/p)"
                : tiltRate > 0.1
                ? "Dịch chuyển chậm (0.1 - 0.4°/p)"
                : "Đứng yên (<0.1°/p)";
        }

        // 5. AI Softmax Breakdown
        const barNormal = document.getElementById("barSegNormal");
        const barAttention = document.getElementById("barSegAttention");
        const barDanger = document.getElementById("barSegDanger");

        if (barNormal) barNormal.style.width = probs.normal.toFixed(1) + "%";
        if (barAttention) barAttention.style.width = probs.attention.toFixed(1) + "%";
        if (barDanger) barDanger.style.width = probs.danger.toFixed(1) + "%";

        const probNormalVal = document.getElementById("probNormalVal");
        const probAttentionVal = document.getElementById("probAttentionVal");
        const probDangerVal = document.getElementById("probDangerVal");

        if (probNormalVal) probNormalVal.textContent = probs.normal.toFixed(1) + "%";
        if (probAttentionVal) probAttentionVal.textContent = probs.attention.toFixed(1) + "%";
        if (probDangerVal) probDangerVal.textContent = probs.danger.toFixed(1) + "%";

        // 6. Update Charts
        if (sensorChartInstance && probChartInstance) {
            const historySlice = readings.slice(0, 25).reverse();
            const labels = historySlice.map((r) => formatTime(r.created_at));

            const angleData = historySlice.map((r) => {
                const rawA = Math.abs(Number(r.tilt_angle_deg) || 0);
                return isCalibrated ? Math.max(0, Math.abs(rawA - zeroAngleOffset)) : rawA;
            });

            sensorChartInstance.data.labels = labels;
            sensorChartInstance.data.datasets[0].data = angleData;
            sensorChartInstance.update("none");

            const probNormalData = [];
            const probAttentionData = [];
            const probDangerData = [];

            historySlice.forEach((r) => {
                const rawA = Math.abs(Number(r.tilt_angle_deg) || 0);
                const effA = isCalibrated ? Math.max(0, Math.abs(rawA - zeroAngleOffset)) : rawA;
                const p = calculateProbabilities({ ...r, tilt_angle_deg: effA });
                probNormalData.push(p.normal);
                probAttentionData.push(p.attention);
                probDangerData.push(p.danger);
            });

            probChartInstance.data.labels = labels;
            probChartInstance.data.datasets[0].data = probNormalData;
            probChartInstance.data.datasets[1].data = probAttentionData;
            probChartInstance.data.datasets[2].data = probDangerData;
            probChartInstance.update("none");
        }

        // 7. Update Telemetry Table (5 cột: Thời gian | Góc | Vận tốc | Cấp rủi ro | Độ tin cậy)
        const tableBody = document.getElementById("telemetryTableBody");
        if (tableBody) {
            const rowsHtml = readings.slice(0, 20).map((r) => {
                const rawA = Math.abs(Number(r.tilt_angle_deg) || 0);
                const effA = isCalibrated ? Math.max(0, Math.abs(rawA - zeroAngleOffset)) : rawA;
                const p = calculateProbabilities({ ...r, tilt_angle_deg: effA });
                const s = r.status || "normal";
                let badgeClass = "tbl-badge-0";
                let badgeText = "Cấp 0 (An toàn)";
                let confVal = p.normal.toFixed(1) + "%";

                if (s === "uncalibrated" || r.is_calibrated === false) {
                    badgeClass = "tbl-badge-uncalibrated";
                    badgeText = "Chờ hiệu chuẩn";
                    confVal = "—";
                } else if (s === "high_risk_in_model" || s === "danger" || s === 2) {
                    badgeClass = "tbl-badge-2";
                    badgeText = "Cấp 2 (Nguy hiểm)";
                    confVal = p.danger.toFixed(1) + "%";
                } else if (s === "attention" || s === 1) {
                    badgeClass = "tbl-badge-1";
                    badgeText = "Cấp 1 (Chú ý)";
                    confVal = p.attention.toFixed(1) + "%";
                }

                const angleDisplay = (s === "uncalibrated" || r.is_calibrated === false)
                    ? `<span>${formatNum(rawA, 2)}°</span> <small style="color:#b45309;" title="Chưa hiệu chuẩn mốc 0°">(thô)</small>`
                    : `<strong>${formatNum(effA, 2)}°</strong> ${isCalibrated ? `<small style="color:#ea580c;font-weight:600;" title="Góc gốc: ${formatNum(rawA, 2)}°">(bù)</small>` : ''}`;

                const rateDisplay = (s === "uncalibrated" || r.is_calibrated === false)
                    ? "—"
                    : `${formatNum(r.tilt_rate_deg_per_min, 3)}°/p`;

                return `<tr>
                    <td>${formatTime(r.created_at)}</td>
                    <td>${angleDisplay}</td>
                    <td>${rateDisplay}</td>
                    <td><span class="tbl-badge ${badgeClass}">${badgeText}</span></td>
                    <td><strong>${confVal}</strong></td>
                </tr>`;
            }).join("");

            tableBody.innerHTML = rowsHtml;
        }

        // 8. Update Statistics
        const totalCount = document.getElementById("totalPacketsCount");
        const statNormal = document.getElementById("statNormalCount");
        const statAttention = document.getElementById("statAttentionCount");
        const statDanger = document.getElementById("statDangerCount");
        const statMaxTilt = document.getElementById("statMaxTilt");

        if (totalCount) totalCount.textContent = readings.length;

        let cntNormal = 0, cntAttention = 0, cntDanger = 0;
        let maxTilt = 0;

        readings.forEach((r) => {
            const rawA = Math.abs(Number(r.tilt_angle_deg) || 0);
            const effA = isCalibrated ? Math.max(0, Math.abs(rawA - zeroAngleOffset)) : rawA;

            const s = r.status || "normal";
            if (s === "high_risk_in_model" || s === "danger" || s === 2) cntDanger++;
            else if (s === "attention" || s === 1) cntAttention++;
            else cntNormal++;

            if (effA > maxTilt) maxTilt = effA;
        });

        if (statNormal) statNormal.textContent = cntNormal;
        if (statAttention) statAttention.textContent = cntAttention;
        if (statDanger) statDanger.textContent = cntDanger;
        if (statMaxTilt) statMaxTilt.textContent = formatNum(maxTilt, 1) + "°";

        // 9. Node and broker chips
        const brokerDot = document.getElementById("brokerStatusDot");
        const brokerText = document.getElementById("brokerStatusText");
        const nodeIdText = document.getElementById("nodeIdText");

        if (brokerDot) brokerDot.setAttribute("data-state", "online");
        if (brokerText) brokerText.textContent = "Trực tuyến";
        if (nodeIdText && latest.device_id) nodeIdText.textContent = latest.device_id;
    }

    // --- API Data Fetching ---
    async function fetchReadings() {
        try {
            const res = await fetch("/api/readings/?limit=60", { cache: "no-store" });
            if (!res.ok) throw new Error("Mã lỗi HTTP: " + res.status);
            const data = await res.json();
            if (typeof data.is_calibrated === "boolean") {
                isCalibrated = data.is_calibrated;
                zeroAngleOffset = Number(data.zero_angle_offset) || 0.0;
                localStorage.setItem("dashboard_is_calibrated", isCalibrated ? "true" : "false");
                localStorage.setItem("dashboard_zero_angle_offset", zeroAngleOffset.toString());
            }
            const items = data.items || [];
            cachedReadings = items;
            updateDashboard(items);
        } catch (err) {
            console.warn("Polling telemetry API:", err.message);
            const brokerDot = document.getElementById("brokerStatusDot");
            const brokerText = document.getElementById("brokerStatusText");
            if (brokerDot) brokerDot.setAttribute("data-state", "offline");
            if (brokerText) brokerText.textContent = "Chờ kết nối...";
        }
    }

    // --- Simulation Functions ---
    window.simulatePacket = async function (level) {
        initAudio();

        let angle, rate, status, label;

        if (level === 2) {
            angle = +(8.2 + Math.random() * 3.5).toFixed(2);
            rate = +(0.75 + Math.random() * 0.4).toFixed(3);
            status = "high_risk_in_model";
            label = "Cấp 2 (Nguy cơ cao / Sập đổ)";
            showToast("Cảnh báo: Phát hiện nguy cơ sập đổ cấp bách!", "error");
        } else if (level === 1) {
            angle = +(4.2 + Math.random() * 1.5).toFixed(2);
            rate = +(0.22 + Math.random() * 0.15).toFixed(3);
            status = "attention";
            label = "Cấp 1 (Cảnh báo chú ý)";
            showToast("Chú ý: Kết cấu có rung động hoặc lún nghiêng.", "warning");
        } else {
            angle = +(1.1 + Math.random() * 0.6).toFixed(2);
            rate = +(0.015 + Math.random() * 0.02).toFixed(3);
            status = "normal";
            label = "Cấp 0 (An toàn)";
            showToast("Hệ thống an toàn, kết cấu ổn định.", "success");
        }

        const simulatedReading = {
            id: Date.now(),
            device_id: "esp32c3_node01",
            tilt_angle_deg: angle,
            tilt_rate_deg_per_min: rate,
            status: status,
            status_label: label,
            created_at: new Date().toISOString()
        };

        cachedReadings.unshift(simulatedReading);
        updateDashboard(cachedReadings);

        try {
            await fetch("/api/readings/", {
                method: "POST",
                headers: {
                    "Content-Type": "application/json",
                    "X-API-Key": window.IOT_API_TOKEN || "demo-satlo-2026"
                },
                body: JSON.stringify({
                    device_id: simulatedReading.device_id,
                    tilt_angle_deg: simulatedReading.tilt_angle_deg,
                    tilt_rate_deg_per_min: simulatedReading.tilt_rate_deg_per_min,
                    status: level
                })
            });
        } catch (e) {
            console.warn("Lưu mô phỏng qua API không thành công:", e);
        }
    };

    // --- Tare / Zero Angle Calibration Handlers ---
    async function calibrateZeroAngle() {
        showToast("Đang gửi lệnh cân bằng mốc 0° xuống ESP32...", "info");
        try {
            const res = await fetch("/api/calibrate/", {
                method: "POST",
                headers: {
                    "Content-Type": "application/json",
                    "X-API-Key": window.IOT_API_TOKEN || "demo-satlo-2026"
                }
            });
            const data = await res.json();
            if (data.ok) {
                isCalibrated = true;
                zeroAngleOffset = latestRawAngle;
                localStorage.setItem("dashboard_zero_angle_offset", zeroAngleOffset.toString());
                localStorage.setItem("dashboard_is_calibrated", "true");
                showToast("Đã gửi lệnh cân bằng mốc 0°! ESP32 sẽ chốt mốc và ghi nhớ vào Flash.", "success");
                await fetchReadings();
            } else {
                showToast(data.error || "Không gửi được lệnh hiệu chuẩn.", "error");
            }
        } catch (e) {
            isCalibrated = true;
            zeroAngleOffset = latestRawAngle;
            localStorage.setItem("dashboard_zero_angle_offset", zeroAngleOffset.toString());
            localStorage.setItem("dashboard_is_calibrated", "true");
            showToast(`Đã cân bằng mốc 0° cục bộ (${formatNum(zeroAngleOffset, 2)}°)`, "success");
            updateDashboard(cachedReadings);
        }
    }

    async function resetCalibration() {
        if (!confirm("Bạn có chắc chắn muốn xóa mốc 0° đã lưu trong Flash và đưa ESP32 về trạng thái chờ hiệu chuẩn không?")) return;
        try {
            const res = await fetch("/api/calibrate/reset/", {
                method: "POST",
                headers: {
                    "Content-Type": "application/json",
                    "X-API-Key": window.IOT_API_TOKEN || "demo-satlo-2026"
                }
            });
            const data = await res.json();
            if (data.ok) {
                zeroAngleOffset = 0.0;
                isCalibrated = false;
                localStorage.removeItem("dashboard_zero_angle_offset");
                localStorage.removeItem("dashboard_is_calibrated");
                showToast("Đã gửi lệnh xóa mốc 0°. Thiết bị quay về trạng thái chờ hiệu chuẩn.", "info");
                await fetchReadings();
            }
        } catch (e) {
            zeroAngleOffset = 0.0;
            isCalibrated = false;
            localStorage.removeItem("dashboard_zero_angle_offset");
            localStorage.removeItem("dashboard_is_calibrated");
            showToast("Đã hủy mốc bù cục bộ.", "info");
            updateDashboard(cachedReadings);
        }
    }

    // --- Clear History Handler ---
    async function clearHistory() {
        if (!confirm("Bạn có chắc chắn muốn xóa toàn bộ lịch sử bản tin quan trắc không?")) return;
        try {
            const res = await fetch("/api/readings/clear/", {
                method: "POST",
                headers: { "X-API-Key": window.IOT_API_TOKEN || "demo-satlo-2026" }
            });
            if (res.ok) {
                cachedReadings = [];
                updateDashboard([]);
                showToast("Đã xóa sạch lịch sử telemetry.", "success");
            }
        } catch (e) {
            showToast("Lỗi kết nối khi xóa dữ liệu.", "error");
        }
    }

    // --- Sound Toggle Handler ---
    function toggleSound() {
        soundEnabled = !soundEnabled;
        const soundText = document.getElementById("soundText");
        if (soundText) {
            soundText.textContent = soundEnabled ? "Còi: Bật" : "Còi: Tắt";
        }
        showToast(soundEnabled ? "Đã bật âm thanh cảnh báo." : "Đã tắt âm thanh cảnh báo.", "info");
        if (soundEnabled) initAudio();
    }

    // --- Initialization on DOM Load ---
    document.addEventListener("DOMContentLoaded", () => {
        initCharts();

        const btnCalibQuick = document.getElementById("btnCalibQuick");
        const btnResetQuick = document.getElementById("btnResetQuick");
        const btnCalibrateZero = document.getElementById("btnCalibrateZero");
        const btnResetCalib = document.getElementById("btnResetCalib");
        const btnClear = document.getElementById("btnClearHistory");
        const btnSound = document.getElementById("soundToggleBtn");

        if (btnCalibQuick) btnCalibQuick.addEventListener("click", calibrateZeroAngle);
        if (btnResetQuick) btnResetQuick.addEventListener("click", resetCalibration);
        if (btnCalibrateZero) btnCalibrateZero.addEventListener("click", calibrateZeroAngle);
        if (btnResetCalib) btnResetCalib.addEventListener("click", resetCalibration);
        if (btnClear) btnClear.addEventListener("click", clearHistory);
        if (btnSound) btnSound.addEventListener("click", toggleSound);

        fetchReadings();
        pollingTimer = setInterval(fetchReadings, 1500);
    });

})();
