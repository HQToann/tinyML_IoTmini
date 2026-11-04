import os
import sys
from django.apps import AppConfig

class MonitoringConfig(AppConfig):
    default_auto_field = 'django.db.models.BigAutoField'
    name = 'monitoring'
    verbose_name = 'Hệ thống giám sát sạt lở đất TinyML'

    def ready(self):
        # Nếu đang chạy kiến trúc Microservices (service mqtt-worker riêng), bỏ qua luồng nội bộ
        if os.environ.get('DISABLE_INTERNAL_MQTT_THREAD') == '1':
            return

        # Chỉ khởi chạy khi thực sự là lệnh runserver (tránh chạy khi migrate, test, showmigrations hay run_mqtt_worker)
        is_runserver = 'runserver' in sys.argv
        if not is_runserver:
            return

        # Tránh khởi chạy luồng kép do Django auto-reload
        is_reloader_child = os.environ.get('RUN_MAIN') == 'true'
        is_noreload = '--noreload' in sys.argv

        if is_reloader_child or is_noreload:
            from monitoring.mqtt_client import start_mqtt_client
            start_mqtt_client()
