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
    return render(request, "telemetry/index.html", {"config": ThresholdConfig.current()})


def norm(value):
    text = unicodedata.normalize("NFD", str(value).strip().lower())
    text = "".join(char for char in text if unicodedata.category(char) != "Mn")
    return "".join(char for char in text if char.isalnum())


def status_value(value):
    return {
        "0": "normal", "normal": "normal", "binhthuong": "normal",
        "1": "attention", "attention": "attention", "chuy": "attention", "canhbao": "attention",
        "2": "high_risk_in_model", "highrisk": "high_risk_in_model",
        "highriskinmodel": "high_risk_in_model", "nguycocao": "high_risk_in_model",
        "nguycocaotrongmohinh": "high_risk_in_model",
    }.get(norm(value))


def number(data, keys, label, low, high):
    value = next((data[key] for key in keys if key in data), None)
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
        "tilt_rate_deg_per_min": reading.tilt_rate_deg_per_min,
        "status": reading.status,
        "status_label": reading.get_status_display(),
        "created_at": reading.created_at.isoformat(),
    }


def config_payload(config):
    return {
        "profile": config.profile_name,
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

        config.profile_name = request.POST.get("profile_name", "Mô hình nhà").strip()[:64] or "Mô hình nhà"
        for field, value in values.items():
            setattr(config, field, value)
        config.save()
        messages.success(request, "Đã lưu ngưỡng nghiêng. ESP32 sẽ nhận cấu hình ở lần đồng bộ tiếp theo.")
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
        return JsonResponse({"items": [serialize(item) for item in Reading.objects.all()[:limit]]})

    token = request.headers.get("X-API-Key", "")
    if not secrets.compare_digest(token, settings.IOT_API_TOKEN):
        return JsonResponse({"error": "API key không hợp lệ."}, status=401)

    try:
        data = json.loads(request.body.decode("utf-8"))
        if not isinstance(data, dict):
            raise ValueError("Body JSON phải là object.")
        angle = number(data, ("tilt_angle_deg", "tilt_angle", "goc_nghieng"), "góc nghiêng", -360, 360)
        rate = number(data, ("tilt_rate_deg_per_min", "tilt_rate", "toc_do_doi_goc"), "tốc độ đổi góc", 0, 1000000)
        state = status_value(data.get("status", data.get("state", data.get("trang_thai", data.get("nhan")))))
        if state is None:
            raise ValueError("status phải là normal, attention, high_risk_in_model hoặc nhãn 0/1/2.")
        device = str(data.get("device_id", "node_01")).strip()[:64] or "node_01"
    except (UnicodeDecodeError, json.JSONDecodeError):
        return JsonResponse({"error": "Body phải là JSON UTF-8 hợp lệ."}, status=400)
    except ValueError as error:
        return JsonResponse({"error": str(error)}, status=400)

    reading = Reading.objects.create(
        device_id=device,
        tilt_angle_deg=angle,
        tilt_rate_deg_per_min=rate,
        status=state,
    )
    return JsonResponse({"ok": True, "reading": serialize(reading)}, status=201)


@csrf_exempt
@require_http_methods(["GET"])
def config_api(request):
    token = request.headers.get("X-API-Key", "")
    if not secrets.compare_digest(token, settings.IOT_API_TOKEN):
        return JsonResponse({"error": "API key không hợp lệ."}, status=401)
    return JsonResponse(config_payload(ThresholdConfig.current()))


@require_http_methods(["GET"])
def health_api(request):
    return JsonResponse({"ok": True, "service": "house-tilt-dashboard"})
