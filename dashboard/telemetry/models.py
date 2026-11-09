from django.db import models


class Reading(models.Model):
    STATUS_CHOICES = [
        ("normal", "Bình thường"),
        ("attention", "Chú ý"),
        ("high_risk_in_model", "Nguy cơ cao"),
        ("uncalibrated", "Chờ hiệu chuẩn"),
    ]

    device_id = models.CharField(max_length=64, default="node_01", db_index=True)
    tilt_angle_deg = models.FloatField()
    roll_deg = models.FloatField(null=True, blank=True)   # nghiêng trái/phải so với mốc 0
    pitch_deg = models.FloatField(null=True, blank=True)  # nghiêng vào/ra so với mốc 0
    tilt_rate_deg_per_min = models.FloatField()
    status = models.CharField(max_length=32, choices=STATUS_CHOICES, db_index=True)
    is_calibrated = models.BooleanField(default=True)
    created_at = models.DateTimeField(auto_now_add=True, db_index=True)

    class Meta:
        ordering = ["-created_at"]

    def __str__(self):
        return f"{self.device_id} | {self.status} | {self.created_at:%Y-%m-%d %H:%M:%S}"


class ThresholdConfig(models.Model):
    """Cấu hình và trạng thái hiệu chuẩn mốc 0°."""

    profile_name = models.CharField(max_length=64, default="Mô hình minh họa")
    attention_angle_deg = models.FloatField(default=10.0)
    high_risk_angle_deg = models.FloatField(default=20.0)
    attention_rate_deg_per_min = models.FloatField(default=5.0)
    high_risk_rate_deg_per_min = models.FloatField(default=15.0)

    # Cờ điều khiển hiệu chuẩn mốc 0° từ website xuống thiết bị
    tare_requested = models.BooleanField(default=False)
    reset_tare_requested = models.BooleanField(default=False)
    is_calibrated = models.BooleanField(default=False)
    zero_angle_offset = models.FloatField(default=0.0)

    updated_at = models.DateTimeField(auto_now=True)

    @classmethod
    def current(cls):
        config, _ = cls.objects.get_or_create(pk=1)
        return config
