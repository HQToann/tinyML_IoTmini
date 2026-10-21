from django.db import migrations, models


class Migration(migrations.Migration):
    initial = True
    dependencies = []
    operations = [
        migrations.CreateModel(
            name="Reading",
            fields=[
                ("id", models.BigAutoField(auto_created=True, primary_key=True, serialize=False, verbose_name="ID")),
                ("device_id", models.CharField(db_index=True, default="node_01", max_length=64)),
                ("soil_moisture_pct", models.FloatField()),
                ("tilt_angle_deg", models.FloatField()),
                ("tilt_rate_deg_per_min", models.FloatField()),
                ("status", models.CharField(choices=[("normal", "Bình thường"), ("attention", "Chú ý"), ("high_risk_in_model", "Nguy cơ cao")], db_index=True, max_length=32)),
                ("created_at", models.DateTimeField(auto_now_add=True, db_index=True)),
            ],
            options={"ordering": ["-created_at"]},
        ),
    ]
