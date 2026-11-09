from django.urls import path
from . import views

urlpatterns = [
    path("", views.dashboard, name="dashboard"),
    path("thong-ke/", views.thong_ke, name="thong_ke"),
    path("thresholds/", views.update_thresholds, name="update_thresholds"),
    path("api/readings/", views.readings_api, name="readings_api"),
    path("api/readings/clear/", views.clear_readings_api, name="clear_readings_api"),
    path("api/calibrate/", views.calibrate_api, name="calibrate_api"),
    path("api/calibrate/reset/", views.reset_calibration_api, name="reset_calibration_api"),
    path("api/config/", views.config_api, name="config_api"),
    path("api/health/", views.health_api, name="health_api"),
]
