from django.shortcuts import render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.db.models import Avg, Max, Count
import json

from monitoring.models import TelemetryRecord
from monitoring.mqtt_client import get_latest_data, update_latest_data

def index(request):
    """Trang chủ hiển thị Dashboard thời gian thực."""
    recent_records = TelemetryRecord.objects.all()[:15]
    total_count = TelemetryRecord.objects.count()
    return render(request, 'monitoring/index.html', {
        'recent_records': recent_records,
        'total_count': total_count,
    })

def api_latest(request):
    """API trả về bản ghi telemetry mới nhất."""
    latest = get_latest_data()
    # Nếu trong cache chưa có gì hoặc vừa khởi động, thử query từ database
    if latest['timestamp'] == '--:--:--':
        last_rec = TelemetryRecord.objects.first()
        if last_rec:
            latest = last_rec.to_dict()
            latest['connected'] = True
    return JsonResponse(latest)

def api_history(request):
    """API trả về danh sách lịch sử các điểm đo phục vụ vẽ biểu đồ (Chart.js)."""
    limit = int(request.GET.get('limit', 30))
    # Lấy các bản ghi mới nhất, sau đó đảo ngược thứ tự để trục thời gian tăng dần
    records = list(TelemetryRecord.objects.all()[:limit])
    records.reverse()

    data = {
        'timestamps': [r.created_at.strftime('%H:%M:%S') for r in records],
        'soil_moisture': [round(r.soil_moisture, 1) for r in records],
        'tilt_angle': [round(r.tilt_angle, 2) for r in records],
        'tilt_rate': [round(r.tilt_rate, 3) for r in records],
        'risk_level': [r.risk_level for r in records],
        'prob_normal': [round(r.prob_normal * 100, 1) for r in records],
        'prob_attention': [round(r.prob_attention * 100, 1) for r in records],
        'prob_high_risk': [round(r.prob_high_risk * 100, 1) for r in records],
        'raw_records': [r.to_dict() for r in reversed(records[:15])],
    }
    return JsonResponse(data)

def api_stats(request):
    """API trả về các chỉ số thống kê tổng quan."""
    total = TelemetryRecord.objects.count()
    normal_count = TelemetryRecord.objects.filter(risk_level=0).count()
    attention_count = TelemetryRecord.objects.filter(risk_level=1).count()
    danger_count = TelemetryRecord.objects.filter(risk_level=2).count()

    aggregates = TelemetryRecord.objects.aggregate(
        avg_moisture=Avg('soil_moisture'),
        max_tilt=Max('tilt_angle'),
        max_rate=Max('tilt_rate')
    )

    return JsonResponse({
        'total_packets': total,
        'normal_count': normal_count,
        'attention_count': attention_count,
        'danger_count': danger_count,
        'avg_moisture': round(aggregates['avg_moisture'] or 0.0, 1),
        'max_tilt': round(aggregates['max_tilt'] or 0.0, 2),
        'max_rate': round(aggregates['max_rate'] or 0.0, 3),
    })

@csrf_exempt
def api_simulate(request):
    """API giả lập gửi dữ liệu để kiểm thử giao diện trực tiếp từ Web."""
    if request.method == 'POST':
        try:
            body = json.loads(request.body.decode('utf-8')) if request.body else {}
            level = int(body.get('level', 0))

            if level == 0:
                moisture, angle, rate, status = 25.5, 2.1, 0.02, 'Bình thường'
                p_norm, p_att, p_high = 0.98, 0.02, 0.0
            elif level == 1:
                moisture, angle, rate, status = 55.0, 14.2, 0.25, 'Chú ý'
                p_norm, p_att, p_high = 0.15, 0.75, 0.10
            else:
                moisture, angle, rate, status = 82.0, 38.5, 0.85, 'Nguy cơ cao'
                p_norm, p_att, p_high = 0.02, 0.10, 0.88

            record = TelemetryRecord.objects.create(
                node_id="simulation_test_node",
                soil_moisture=moisture,
                tilt_angle=angle,
                tilt_rate=rate,
                risk_level=level,
                status=status,
                prob_normal=p_norm,
                prob_attention=p_att,
                prob_high_risk=p_high,
            )
            update_latest_data(record.to_dict())
            return JsonResponse({'success': True, 'record': record.to_dict()})
        except Exception as e:
            return JsonResponse({'success': False, 'error': str(e)}, status=400)
    return JsonResponse({'error': 'POST only'}, status=405)

@csrf_exempt
def api_clear(request):
    """Xóa lịch sử đo đạc."""
    if request.method == 'POST':
        TelemetryRecord.objects.all().delete()
        return JsonResponse({'success': True})
    return JsonResponse({'error': 'POST only'}, status=405)
