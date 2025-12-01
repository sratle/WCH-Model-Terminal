# 配件扩展模块

磁吸式热插拔配件模块，满足不同用户的差异化需求，统一采用CH32V103裸机开发。

## 优先开发模块（P0）

- **fingerprint-model**: 指纹识别模块（本地匹配，仅上报认证结果）
- **knob-model**: 旋钮模块（EC11编码器，音量/参数调节）
- **health-model**: 健康监测模块（心率/血氧/温湿度传感器）

## 后续扩展模块

- **NFC-model**: NFC读卡模块（PN532/RC522）
- **touch-ring-model**: 触摸圆环模块（电容触摸，类旋钮交互）
- **RGB-model**: RGB点阵模块（8×8或16×16，显示图标/动画）
- **distance-model**: 红外测距模块（VL53L0X ToF传感器）
- **screen-model**: 副屏模块（小OLED/LED数码管，显示状态信息）

所有模块支持热插拔，通过UART（115200bps）与核心底板通信。
