import os, sys
os.environ.setdefault("DJANGO_SETTINGS_MODULE", "iot_dashboard.settings")
from django.core.management import execute_from_command_line
if __name__ == "__main__":
    execute_from_command_line(sys.argv)
