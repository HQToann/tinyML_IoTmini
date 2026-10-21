from django.contrib import admin
from monitoring.models import TelemetryRecord

@admin.register(TelemetryRecord)
class TelemetryRecordAdmin(admin.ModelAdmin):
    list_display = ('created_at', 'node_id', 'risk_level', 'status', 'soil_moisture', 'tilt_angle', 'tilt_rate')
    list_filter = ('risk_level', 'status', 'node_id')
    search_fields = ('node_id', 'status')
    readonly_fields = ('created_at',)
    ordering = ('-created_at',)
