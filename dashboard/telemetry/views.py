import json
import math
import secrets
import unicodedata

from django.conf import settings
from django.contrib import messages
from django.http import JsonResponse, HttpResponseRedirect
from django.shortcuts import render
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.http import require_http_methods, require_POST

from .models import Reading, ThresholdConfig


def dashboard(request):
    return render(request, "telemetry/index.html", {
        "config": ThresholdConfig.current(),
    })

def thong_ke(request):
    return render(request, "telemetry/dashboard.html", {
        "api_token": getattr(settings, "IOT_API_TOKEN", "demo-satlo-2026"),
        "config": ThresholdConfig.current(),
    })


@csrf_exempt
@require_POST
def clear_readings_api(request):
    Reading.objects.all().delete()
    return JsonResponse({"ok": True, "message": "Đã xóa toàn bộ lịch sử đo."})

def norm(value):
    text = unicodedata.normalize("NFD", str(value).strip().lower())
    text = "".join(c for c in text if unicodedata.category(c) != "Mn")
    return "".join(c for c in text if c.isalnum() or c == "-")


def status_value(value):
    n = norm(value)
    return {
        "0": "normal", "normal": "normal", "binhthuong": "normal",
        "1": "attention", "attention": "attention", "chuy": "attention", "canhbao": "attention",
        "2": "high_risk_in_model", "highrisk": "high_risk_in_model",
        "nguycocao": "high_risk_in_model",
        "nguycocaotrongmohinh": "high_risk_in_model",
        "-1": "uncalibrated", "uncalibrated": "uncalibrated",
        "chohieuchuan": "uncalibrated", "chua_hieu_chuan": "uncalibrated",
    }.get(n)


def number(data, keys, label, low, high):
    value = next((data[k] for k in keys if k in data), None)
    try:
        value = float(value)
    except (TypeError, ValueError):
        raise ValueError(f"Trường {label} phải là số.")
    if not math.isfinite(value) or not low <= value <= high:
        raise ValueError(f"Trường {label} phải trong khoảng {low} đến {high}.")
    return value


def serialize(reading):
    return {
        "id": reading.id,
        "device_id": reading.device_id,
        "tilt_angle_deg": reading.tilt_angle_deg,
        "roll_deg": reading.roll_deg,
        "pitch_deg": reading.pitch_deg,
        "tilt_rate_deg_per_min": reading.tilt_rate_deg_per_min,
        "status": reading.status,
        "status_label": reading.get_status_display(),
        "is_calibrated": reading.is_calibrated,
        "created_at": reading.created_at.isoformat(),
    }


def config_payload(config):
    return {
        "profile": config.profile_name,
        "is_calibrated": config.is_calibrated,
        "zero_angle_offset": config.zero_angle_offset,
        "thresholds": {
            "attention_angle_deg": config.attention_angle_deg,
            "high_risk_angle_deg": config.high_risk_angle_deg,
            "attention_rate_deg_per_min": config.attention_rate_deg_per_min,
            "high_risk_rate_deg_per_min": config.high_risk_rate_deg_per_min,
        },
        "updated_at": config.updated_at.isoformat(),
    }


@require_POST
def update_thresholds(request):
    config = ThresholdConfig.current()
    try:
        values = {
            "attention_angle_deg": number(request.POST, ("attention_angle_deg",), "góc chú ý", 0, 360),
            "high_risk_angle_deg": number(request.POST, ("high_risk_angle_deg",), "góc nguy cơ cao", 0, 360),
            "attention_rate_deg_per_min": number(request.POST, ("attention_rate_deg_per_min",), "tốc độ chú ý", 0, 1000000),
            "high_risk_rate_deg_per_min": number(request.POST, ("high_risk_rate_deg_per_min",), "tốc độ nguy cơ cao", 0, 1000000),
        }
        if values["attention_angle_deg"] >= values["high_risk_angle_deg"]:
            raise ValueError("Góc nguy cơ cao phải lớn hơn góc chú ý.")
        if values["attention_rate_deg_per_min"] >= values["high_risk_rate_deg_per_min"]:
            raise ValueError("Tốc độ nguy cơ cao phải lớn hơn tốc độ chú ý.")

        config.profile_name = request.POST.get("profile_name", "Mô hình minh họa").strip()[:64] or "Mô hình minh họa"
        for field, value in values.items():
            setattr(config, field, value)
        config.save()
        messages.success(request, "Đã lưu cấu hình thành công.")
    except ValueError as error:
        messages.error(request, str(error))

    return HttpResponseRedirect("/")


@csrf_exempt
@require_http_methods(["GET", "POST"])
def readings_api(request):
    if request.method == "GET":
        try:
            limit = min(max(int(request.GET.get("limit", "60")), 1), 200)
        except ValueError:
            return JsonResponse({"error": "limit phải là số nguyên."}, status=400)
        config = ThresholdConfig.current()
        return JsonResponse({
            "is_calibrated": config.is_calibrated,
            "zero_angle_offset": config.zero_angle_offset,
            "items": [serialize(r) for r in Reading.objects.all()[:limit]]
        })

    token = request.headers.get("X-API-Key", "")
    if not secrets.compare_digest(token, settings.IOT_API_TOKEN):
        return JsonResponse({"error": "API key không hợp lệ."}, status=401)

    try:
        data = json.loads(request.body.decode("utf-8"))
        if not isinstance(data, dict):
            raise ValueError("Body JSON phải là object.")
        angle = number(data, ("tilt_angle_deg", "tilt_angle", "goc_nghieng"), "góc nghiêng", -360, 360)
        rate = number(data, ("tilt_rate_deg_per_min", "tilt_rate", "toc_do_doi_goc"), "tốc độ đổi góc", 0, 1000000)
        roll = number(data, ("roll_deg", "roll"), "roll", -360, 360) if ("roll_deg" in data or "roll" in data) else None
        pitch = number(data, ("pitch_deg", "pitch"), "pitch", -360, 360) if ("pitch_deg" in data or "pitch" in data) else None
        state = status_value(data.get("status", data.get("state", data.get("trang_thai", data.get("nhan")))))
        if state is None:
            raise ValueError("status phải là normal, attention, high_risk_in_model, uncalibrated hoặc nhãn 0/1/2/-1.")
        device = str(data.get("device_id", "node_01")).strip()[:64] or "node_01"
        is_calib = bool(data.get("is_calibrated", state != "uncalibrated"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return JsonResponse({"error": "Body phải là JSON UTF-8 hợp lệ."}, status=400)
    except ValueError as error:
        return JsonResponse({"error": str(error)}, status=400)

    reading = Reading.objects.create(
        device_id=device,
        tilt_angle_deg=angle,
        roll_deg=roll,
        pitch_deg=pitch,
        tilt_rate_deg_per_min=rate,
        status=state,
        is_calibrated=is_calib,
    )

    # Kiểm tra xem có lệnh tare (cân bằng 0°) từ Dashboard gửi xuống ESP32 không
    config = ThresholdConfig.current()
    tare_cmd = config.tare_requested
    reset_cmd = config.reset_tare_requested

    # Khi gửi lệnh xuống ESP32, xóa cờ ngay lập tức để ESP32 chỉ thực hiện 1 lần, tránh lặp vô tận
    if tare_cmd:
        config.tare_requested = False
        config.is_calibrated = True
        config.zero_angle_offset = angle
        config.save()
    elif reset_cmd:
        config.reset_tare_requested = False
        config.is_calibrated = False
        config.zero_angle_offset = 0.0
        config.save()
    elif is_calib != config.is_calibrated:
        config.is_calibrated = is_calib
        config.save()

    return JsonResponse({
        "ok": True,
        "reading": serialize(reading),
        "tare_command": tare_cmd,
        "reset_tare_command": reset_cmd
    }, status=201, json_dumps_params={"separators": (",", ":")})


@csrf_exempt
@require_POST
def calibrate_api(request):
    """API để Dashboard gửi lệnh cân bằng mốc 0° xuống thiết bị."""
    config = ThresholdConfig.current()
    config.tare_requested = True
    config.reset_tare_requested = False
    # KHÔNG set is_calibrated=True ở đây - phải chờ ESP32 xác nhận
    config.save()
    return JsonResponse({
        "ok": True,
        "message": "Đã gửi lệnh cân bằng mốc 0° xuống thiết bị. ESP32 sẽ chốt mốc và lưu vào Flash ở bản tin kế tiếp."
    })


@csrf_exempt
@require_POST
def reset_calibration_api(request):
    """API để Dashboard gửi lệnh xóa mốc bù, trở về góc gốc."""
    config = ThresholdConfig.current()
    config.reset_tare_requested = True
    config.tare_requested = False
    config.is_calibrated = False
    config.zero_angle_offset = 0.0
    config.save()
    return JsonResponse({
        "ok": True,
        "message": "Đã gửi lệnh đặt lại mốc gốc xuống thiết bị."
    })


@csrf_exempt
@require_http_methods(["GET"])
def config_api(request):
    token = request.headers.get("X-API-Key", "")
    if not secrets.compare_digest(token, settings.IOT_API_TOKEN):
        return JsonResponse({"error": "API key không hợp lệ."}, status=401)
    return JsonResponse(config_payload(ThresholdConfig.current()))


@require_http_methods(["GET"])
def health_api(request):
    return JsonResponse({"ok": True, "service": "landslide-dashboard"})
