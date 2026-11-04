import os
import sys
from django.apps import AppConfig

class MonitoringConfig(AppConfig):
    default_auto_field = 'django.db.models.BigAutoField'
    name = 'monitoring'
    verbose_name = 'Hệ thống giám sát sạt lở đất TinyML'

    def ready(self):
        # Tránh khởi chạy luồng kép do Django auto-reload
        is_runserver = 'runserver' in sys.argv
        is_main_process = os.environ.get('RUN_MAIN') == 'true' or not is_runserver

        if is_main_process:
            from monitoring.mqtt_client import start_mqtt_client
            start_mqtt_client()
