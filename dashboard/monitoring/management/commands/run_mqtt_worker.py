import json
import logging
import signal
import sys
import time
import uuid
from django.core.management.base import BaseCommand
from django.conf import settings
from django.utils import timezone
from monitoring.models import TelemetryRecord

logger = logging.getLogger(__name__)

class Command(BaseCommand):
    help = 'Khởi chạy tiến trình nền chuyên thu thập dữ liệu MQTT từ ESP32 (Microservice Worker)'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.running = True

    def handle(self, *args, **options):
        # Đăng ký bắt tín hiệu ngắt khi container dừng
        signal.signal(signal.SIGINT, self._sig_handler)
        signal.signal(signal.SIGTERM, self._sig_handler)

        self.stdout.write(self.style.SUCCESS('====================================================='))
        self.stdout.write(self.style.SUCCESS(' [MQTT WORKER] BẮT ĐẦU DỊCH VỤ THU THẬP DỮ LIỆU IOT   '))
        self.stdout.write(self.style.SUCCESS('====================================================='))
        self.stdout.write(f"Broker mục tiêu: {settings.MQTT_BROKER_HOST}:{settings.MQTT_BROKER_PORT}")
        self.stdout.write(f"Chủ đề (Topic) : {settings.MQTT_TOPIC}")

        import paho.mqtt.client as mqtt

        client_id = f"{settings.MQTT_CLIENT_ID}_worker_{uuid.uuid4().hex[:6]}"

        try:
            client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
        except Exception:
            client = mqtt.Client(client_id=client_id)

        def on_connect(c, userdata, flags, rc, properties=None):
            is_success = (rc == 0) or (getattr(rc, 'value', None) == 0) or (hasattr(rc, 'is_failure') and not rc.is_failure)
            if is_success:
                self.stdout.write(self.style.SUCCESS(f"[MQTT Worker] Đã kết nối thành công tới broker {settings.MQTT_BROKER_HOST}:{settings.MQTT_BROKER_PORT}"))
                c.subscribe(settings.MQTT_TOPIC)
                self.stdout.write(self.style.SUCCESS(f"[MQTT Worker] Đã đăng ký lắng nghe topic: '{settings.MQTT_TOPIC}'"))
            else:
                self.stdout.write(self.style.ERROR(f"[MQTT Worker] Kết nối thất bại với mã lỗi: {rc}"))

        def on_message(c, userdata, msg):
            try:
                payload_str = msg.payload.decode('utf-8')
                payload = json.loads(payload_str)

                if not isinstance(payload, dict):
                    self.stdout.write(self.style.WARNING(f"[MQTT Worker] Bỏ qua bản tin không phải JSON dict: {payload_str[:50]}"))
                    return

                node_id = payload.get('node', 'esp32c3_landslide_node01')
                soil_moisture = float(payload.get('soil_moisture', 0.0))
                tilt_angle = float(payload.get('tilt_angle', 0.0))
                tilt_rate = float(payload.get('tilt_rate', 0.0))
                risk_level = int(payload.get('risk_level', 0))
                status = str(payload.get('status', 'Bình thường'))

                prob_normal = float(payload.get('prob_normal', 1.0))
                prob_attention = float(payload.get('prob_attention', 0.0))
                prob_high_risk = float(payload.get('prob_high_risk', 0.0))

                # Lưu bản ghi vào SQLite Database
                record = TelemetryRecord.objects.create(
                    node_id=node_id,
                    soil_moisture=soil_moisture,
                    tilt_angle=tilt_angle,
                    tilt_rate=tilt_rate,
                    risk_level=risk_level,
                    status=status,
                    prob_normal=prob_normal,
                    prob_attention=prob_attention,
                    prob_high_risk=prob_high_risk,
                )

                local_ts = timezone.localtime(record.created_at).strftime('%H:%M:%S')
                self.stdout.write(
                    f"[{local_ts}] [MQTT Worker] Bản tin #{record.id} từ {node_id}: "
                    f"Góc={tilt_angle}°, Tốc độ={tilt_rate}°/phút, Cấp={risk_level} ({status})"
                )

                # Tự động cắt tỉa database định kỳ: Giữ 5000 bản ghi mới nhất
                if record.id % 200 == 0:
                    total_records = TelemetryRecord.objects.count()
                    if total_records > 5000:
                        cutoff_id = TelemetryRecord.objects.order_by('-id')[5000].id
                        deleted_count, _ = TelemetryRecord.objects.filter(id__lt=cutoff_id).delete()
                        self.stdout.write(self.style.WARNING(f"[DB Pruning] Đã dọn dẹp {deleted_count} bản ghi cũ, duy trì 5000 bản tin mới nhất."))

            except Exception as e:
                self.stdout.write(self.style.ERROR(f"[MQTT Worker Lỗi] {e}"))

        client.on_connect = on_connect
        client.on_message = on_message

        while self.running:
            try:
                self.stdout.write(f"[MQTT Worker] Đang kết nối tới {settings.MQTT_BROKER_HOST}:{settings.MQTT_BROKER_PORT}...")
                client.connect(settings.MQTT_BROKER_HOST, settings.MQTT_BROKER_PORT, keepalive=60)
                client.loop_forever()
            except Exception as ex:
                if not self.running:
                    break
                self.stdout.write(self.style.WARNING(f"[MQTT Worker] Mất kết nối ({ex}). Tự động thử lại sau 5s..."))
                time.sleep(5)

        self.stdout.write(self.style.SUCCESS("[MQTT Worker] Dịch vụ đã dừng an toàn."))

    def _sig_handler(self, signum, frame):
        self.stdout.write(self.style.WARNING(f"\n[MQTT Worker] Nhận tín hiệu ngắt ({signum}), đang dừng tiến trình..."))
        self.running = False
        sys.exit(0)
