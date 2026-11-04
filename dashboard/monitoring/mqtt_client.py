import json
import logging
import threading
import time
from django.conf import settings

logger = logging.getLogger(__name__)

# Cache chứa bản tin mới nhất trong bộ nhớ
LATEST_DATA_LOCK = threading.Lock()
LATEST_DATA = {
    'node_id': 'esp32c3_landslide_node01',
    'soil_moisture': 0.0,
    'tilt_angle': 0.0,
    'tilt_rate': 0.0,
    'risk_level': 0,
    'status': 'Chờ kết nối...',
    'prob_normal': 100.0,
    'prob_attention': 0.0,
    'prob_high_risk': 0.0,
    'timestamp': '--:--:--',
    'date': '--/--/----',
    'connected': False,
    'last_seen_seconds': None,
}

_last_packet_time = 0

def get_latest_data():
    global _last_packet_time
    with LATEST_DATA_LOCK:
        data = dict(LATEST_DATA)
        if _last_packet_time > 0:
            data['last_seen_seconds'] = int(time.time() - _last_packet_time)
        return data

def update_latest_data(new_record):
    global _last_packet_time
    with LATEST_DATA_LOCK:
        LATEST_DATA.update(new_record)
        LATEST_DATA['connected'] = True
        _last_packet_time = time.time()

def on_connect(client, userdata, flags, rc, properties=None):
    # Hỗ trợ cả paho-mqtt v1 (int) và v2 (ReasonCode)
    is_success = (rc == 0) or (getattr(rc, 'value', None) == 0) or (hasattr(rc, 'is_failure') and not rc.is_failure)
    if is_success:
        print(f"[MQTT] Đã kết nối thành công tới broker {settings.MQTT_BROKER_HOST}:{settings.MQTT_BROKER_PORT}", flush=True)
        logger.info(f"MQTT Connected successfully to {settings.MQTT_BROKER_HOST}")
        client.subscribe(settings.MQTT_TOPIC)
        print(f"[MQTT] Đã đăng ký lắng nghe topic: {settings.MQTT_TOPIC}", flush=True)
        logger.info(f"Subscribed to topic: {settings.MQTT_TOPIC}")
    else:
        print(f"[MQTT] Kết nối MQTT thất bại với mã lỗi: {rc}", flush=True)
        logger.error(f"MQTT Connection failed with return code: {rc}")

def on_message(client, userdata, msg):
    try:
        payload_str = msg.payload.decode('utf-8')
        payload = json.loads(payload_str)

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
        from monitoring.models import TelemetryRecord
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

        # Cập nhật cache thời gian thực
        update_latest_data(record.to_dict())
        print(f"[MQTT] Nhận dữ liệu ID={record.id} từ {node_id}: Đất={soil_moisture}%, Góc={tilt_angle}°, Cấp={risk_level} ({status})", flush=True)

    except Exception as e:
        print(f"[MQTT Error] Lỗi xử lý gói tin MQTT: {e}", flush=True)
        logger.error(f"Lỗi xử lý gói tin MQTT: {e}")

_mqtt_thread = None

def start_mqtt_client():
    global _mqtt_thread
    if _mqtt_thread is not None and _mqtt_thread.is_alive():
        return

    def run():
        import paho.mqtt.client as mqtt
        import uuid

        # Thêm chuỗi ngẫu nhiên tránh xung đột Client ID trên public broker
        client_id = f"{settings.MQTT_CLIENT_ID}_{uuid.uuid4().hex[:6]}"

        # Hỗ trợ cả paho-mqtt v1 và v2
        try:
            client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
        except Exception:
            client = mqtt.Client(client_id=client_id)

        client.on_connect = on_connect
        client.on_message = on_message

        while True:
            try:
                print(f"[MQTT] Đang kết nối tới MQTT Broker {settings.MQTT_BROKER_HOST}:{settings.MQTT_BROKER_PORT} (Client ID: {client_id})...", flush=True)
                logger.info(f"Connecting to MQTT Broker {settings.MQTT_BROKER_HOST}:{settings.MQTT_BROKER_PORT}...")
                client.connect(settings.MQTT_BROKER_HOST, settings.MQTT_BROKER_PORT, keepalive=60)
                client.loop_forever()
            except Exception as ex:
                print(f"[MQTT Warning] Mất kết nối MQTT: {ex}. Thử kết nối lại sau 5s...", flush=True)
                logger.warning(f"MQTT connection lost: {ex}. Reconnecting in 5s...")
                time.sleep(5)

    _mqtt_thread = threading.Thread(target=run, daemon=True, name="MQTT_Telemetry_Worker")
    _mqtt_thread.start()
    print("[MQTT] Luồng thu thập dữ liệu MQTT đã được khởi động thành công.", flush=True)
    logger.info("MQTT Telemetry worker thread started.")
