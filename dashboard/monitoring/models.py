from django.db import models
from django.utils import timezone

class TelemetryRecord(models.Model):
    RISK_LEVEL_CHOICES = [
        (0, 'Bình thường (An toàn)'),
        (1, 'Chú ý (Cảnh báo)'),
        (2, 'Nguy cơ cao (Nguy hiểm)'),
    ]

    node_id = models.CharField(max_length=64, default="esp32c3_landslide_node01", verbose_name="Mã trạm cảm biến")
    soil_moisture = models.FloatField(verbose_name="Độ ẩm đất (%)")
    tilt_angle = models.FloatField(verbose_name="Góc nghiêng (°)")
    tilt_rate = models.FloatField(verbose_name="Tốc độ đổi góc (°/phút)")
    risk_level = models.IntegerField(choices=RISK_LEVEL_CHOICES, default=0, verbose_name="Mức rủi ro TinyML")
    status = models.CharField(max_length=64, default="Bình thường", verbose_name="Trạng thái phân loại")
    prob_normal = models.FloatField(default=1.0, verbose_name="Xác suất an toàn")
    prob_attention = models.FloatField(default=0.0, verbose_name="Xác suất chú ý")
    prob_high_risk = models.FloatField(default=0.0, verbose_name="Xác suất nguy cơ cao")
    created_at = models.DateTimeField(auto_now_add=True, db_index=True, verbose_name="Thời gian nhận")

    class Meta:
        ordering = ['-created_at']
        verbose_name = "Bản tin cảm biến"
        verbose_name_plural = "Lịch sử giám sát cảm biến"

    def __str__(self):
        return f"[{self.created_at.strftime('%H:%M:%S')}] {self.node_id} - Mức {self.risk_level} ({self.status})"

    def to_dict(self):
        return {
            'id': self.id,
            'node_id': self.node_id,
            'soil_moisture': round(self.soil_moisture, 1),
            'tilt_angle': round(self.tilt_angle, 2),
            'tilt_rate': round(self.tilt_rate, 3),
            'risk_level': self.risk_level,
            'status': self.status,
            'prob_normal': round(self.prob_normal * 100, 1),
            'prob_attention': round(self.prob_attention * 100, 1),
            'prob_high_risk': round(self.prob_high_risk * 100, 1),
            'timestamp': self.created_at.strftime('%H:%M:%S'),
            'date': self.created_at.strftime('%d/%m/%Y'),
        }
