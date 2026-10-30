from django.db import migrations


class Migration(migrations.Migration):
    dependencies = [("telemetry", "0002_thresholdconfig")]

    operations = [
        migrations.RemoveField(model_name="reading", name="soil_moisture_pct"),
        migrations.RemoveField(model_name="thresholdconfig", name="moisture_attention_pct"),
    ]

