# Dockerfile - Web Dashboard Giám Sát Sạt Lở Đất (Django + MQTT)

# Sử dụng Python 3.11 Slim siêu nhẹ và ổn định
FROM python:3.11-slim

# Thiết lập thư mục làm việc trong container
WORKDIR /app

# Ngăn Python tạo bytecode .pyc và bật in log trực tiếp ra console
ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1

# Cài đặt các thư viện cần thiết trước để tận dụng Docker Cache layer
COPY requirements.txt /app/
RUN pip install --no-cache-dir -r requirements.txt

# Sao chép toàn bộ mã nguồn của dashboard vào container
COPY . /app/

# Mở cổng 8000 phục vụ giao diện Web
EXPOSE 8000

# Tự động migrate database và khởi chạy Django Server
CMD ["sh", "-c", "python manage.py migrate && python manage.py runserver 0.0.0.0:8000"]

