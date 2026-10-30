from django.db import models


class Reading(models.Model):
    STATUS_CHOICES = [
        ("normal", "Bình thường"),
        ("attention", "Chú ý"),
        ("high_risk_in_model", "Nguy cơ cao trong mô hình"),
    ]

    device_id = models.CharField(max_length=64, default="node_01", db_index=True)
    tilt_angle_deg = models.FloatField()
    tilt_rate_deg_per_min = models.FloatField()
    status = models.CharField(max_length=32, choices=STATUS_CHOICES, db_index=True)
    created_at = models.DateTimeField(auto_now_add=True, db_index=True)

    class Meta:
        ordering = ["-created_at"]

    def __str__(self):
        return f"{self.device_id} | {self.status} | {self.created_at:%Y-%m-%d %H:%M:%S}"


class ThresholdConfig(models.Model):
    """Nguong canh bao bo sung cho goc va toc do nghieng."""

    profile_name = models.CharField(max_length=64, default="Mô hình nhà")
    attention_angle_deg = models.FloatField(default=10.0)
    high_risk_angle_deg = models.FloatField(default=20.0)
    attention_rate_deg_per_min = models.FloatField(default=5.0)
    high_risk_rate_deg_per_min = models.FloatField(default=15.0)
    updated_at = models.DateTimeField(auto_now=True)

    @classmethod
    def current(cls):
        config, _ = cls.objects.get_or_create(pk=1)
        return config
