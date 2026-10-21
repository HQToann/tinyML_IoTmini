from django.db import migrations, models


class Migration(migrations.Migration):
    dependencies = [("telemetry", "0001_initial")]

    operations = [
        migrations.CreateModel(
            name="ThresholdConfig",
            fields=[
                ("id", models.BigAutoField(auto_created=True, primary_key=True, serialize=False, verbose_name="ID")),
                ("profile_name", models.CharField(default="Mô hình minh họa", max_length=64)),
                ("moisture_attention_pct", models.FloatField(default=70.0)),
                ("attention_angle_deg", models.FloatField(default=10.0)),
                ("high_risk_angle_deg", models.FloatField(default=20.0)),
                ("attention_rate_deg_per_min", models.FloatField(default=5.0)),
                ("high_risk_rate_deg_per_min", models.FloatField(default=15.0)),
                ("updated_at", models.DateTimeField(auto_now=True)),
            ],
        ),
    ]
