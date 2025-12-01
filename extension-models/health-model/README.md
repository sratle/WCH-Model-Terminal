# 健康监测模块 (health-model)

多传感器健康监测模块，支持心率、血氧、温湿度检测。

## 硬件配置
- **主控**: CH32V103 (裸机)
- **传感器**:
  - MAX30102: 心率/血氧 (PPG)
  - SHT30/AHT20: 温湿度
  - 可选: 加速度计、陀螺仪

## 功能特性
- 实时心率监测
- 血氧饱和度 (SpO2)
- 环境温湿度
- 运动检测 (可选)
- 本地滤波算法

## 通信协议
- SensorRead: 读取传感器数据
- StreamStart/Stop: 启动/停止连续采样
- Calibrate: 校准传感器

## 数据输出
- HeartRate: 心率 (bpm)
- SpO2: 血氧饱和度 (%)
- Temperature: 温度 (°C)
- Humidity: 湿度 (%)
