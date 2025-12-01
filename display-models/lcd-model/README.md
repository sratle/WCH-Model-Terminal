# TFT液晶屏模块 (lcd-model)

7寸TFT彩色液晶屏，800×480分辨率，≥24fps刷新率。

## 硬件配置
- **主控**: CH32H417 (优先) 或 CH32V307QFN68
- **屏幕**: 7寸 800×480 RGB接口
- **外扩存储**:
  - W25Q128 (16MB SPI Flash) - UI资源缓存
  - W9825G6KH (32MB SDRAM) - LVGL显存

## 驱动方案
- **接口**: LTDC直接驱动RGB888
- **刷新策略**: 双缓冲 + VSYNC + 脏矩形
- **色彩格式**: RGB565/RGB888

## 软件架构 (FreeRTOS + LVGL)
- CommParser: UART指令解析
- UIRenderer: LVGL渲染引擎
- DisplayDriver: 屏幕刷新与DMA
- ResourceMgr: 资源预加载与缓存
