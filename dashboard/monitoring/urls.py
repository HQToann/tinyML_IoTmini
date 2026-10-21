from django.urls import path
from monitoring import views

urlpatterns = [
    path('', views.index, name='index'),
    path('api/latest/', views.api_latest, name='api_latest'),
    path('api/history/', views.api_history, name='api_history'),
    path('api/stats/', views.api_stats, name='api_stats'),
    path('api/simulate/', views.api_simulate, name='api_simulate'),
    path('api/clear/', views.api_clear, name='api_clear'),
]
