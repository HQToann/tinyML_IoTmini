from django.urls import path
from . import views

urlpatterns = [
    path("", views.dashboard, name="dashboard"),
    path("thresholds/", views.update_thresholds, name="update_thresholds"),
    path("api/readings/", views.readings_api, name="readings_api"),
    path("api/config/", views.config_api, name="config_api"),
    path("api/health/", views.health_api, name="health_api"),
]
