# MiniUI 游戏应用编写指南

本文档总结在 MiniUI 框架上开发游戏类应用时的关键注意事项，基于 Touch Ball 游戏开发过程中踩过的坑。

---

## 1. 游戏页面架构

### 使用 `ui_app_page_init` 创建页面

游戏页面基于 `ui_app_page_t`，它提供标题栏、返回按钮等通用组件：

```c
ui_app_page_t s_game_mygame;
ui_widget_t *s_mygame_widgets[3];  // touch_area + title + back_button

void game_mygame_init(void)
{
    ui_app_page_init(&s_game_mygame, "My Game", 0x205);
    s_game_mygame.page.flags |= UI_PAGE_FLAG_GAME;  // 必须设置！
    // ...
    ui_page_set_widgets(&s_game_mygame.page, s_mygame_widgets, 3);
    ui_page_set_callbacks(&s_game_mygame.page, mygame_enter, NULL,
                          mygame_draw, NULL);
    ui_page_register(&s_game_mygame.page);
}
```

### 必须设置 `UI_PAGE_FLAG_GAME`

**坑**：如果不设置此标志，`UI_Tick` 不会每帧调用 `on_draw`，游戏逻辑不会更新，页面也不会重绘。

```c
s_game_mygame.page.flags |= UI_PAGE_FLAG_GAME;  // 必须！
```

---

## 2. 局部刷新（Dirty Rect）机制

### 核心原则：只标记变化的区域，尽量避免全刷

全屏刷新（800x480 = 384K像素）在 FMC CPU 模式下需要 ~107ms（7 FPS）。局部刷新只重绘变化区域，可将数据量降低 90%+。

**尽量使用局刷**：全刷（`need_full_redraw = true`）应仅用于状态切换（开始游戏、游戏结束等），游戏正常运行时应该始终使用局刷。即使相机移动、多个实体同时运动，也可以通过精确标记脏区域来避免全刷。脏区域数量多一些没关系（最多 256 个，超出会退化为全刷），标记多一点比漏标导致残影要好。

### 游戏页面的特殊刷新流程

`UI_PAGE_FLAG_GAME` 页面的刷新流程与普通页面不同：

- **普通页面**：`UI_Tick` → `ui_page_draw()` → 根据 dirty list 调用 `on_draw` + 绘制 widgets
- **游戏页面**：`UI_Tick` → 直接调用 `on_draw` → 游戏自行管理 dirty regions 和绘制

因此游戏页面的 `on_draw` 回调需要自行处理：
1. 更新游戏逻辑
2. 标记脏区域（`ui_page_invalidate`）
3. 读取脏区域列表（`ui_page_get_dirty_list`）
4. 只重绘脏区域内的内容
5. 清除脏列表（`ui_page_clear_dirty`）

### 标记脏区域的时机

```c
// 球移动：标记旧位置和新位置
tb_invalidate_ball(old_x, old_y, r);
tb_invalidate_ball(new_x, new_y, r);

// HUD 文字变化
tb_invalidate_hud();

// 状态切换：全屏重绘
s_tb.need_full_redraw = true;
```

### 单遍原子绘制法（防闪烁核心）

**坑**：如果先擦除所有脏区域再画球（两遍绘制），擦除和重绘之间 LCD 可能扫描到空白帧，导致闪烁。

正确做法是**单遍原子绘制**：每个脏区域内，擦除后立即重绘，不留间隔：

```c
ui_rect_t game_area = {0, TITLE_BAR_H, SCREEN_W, Screen_H - TITLE_BAR_H};
ui_rect_t hud = {HUD_X, HUD_Y, HUD_W, HUD_H};
bool hud_needs_redraw = false;

for (each dirty rect) {
    // 1. 精确 clip 内擦除背景
    ui_render_set_clip(&clip);
    ui_draw_fill_rect(&clip, UI_COLOR_BG_MAIN);

    // 2. 游戏区域 clip 内重绘球（球可能略超出脏区域，但没关系）
    ui_render_set_clip(&game_area);
    tb_draw_balls_in_rect(&clip);

    // 3. 记录 HUD 是否需要重绘（不要在循环内立即重绘！）
    if (rects_overlap(&clip, &hud)) {
        hud_needs_redraw = true;
    }
}

// 4. 循环结束后统一重绘 HUD 一次（先擦后画）
if (hud_needs_redraw) {
    ui_render_set_clip(&hud);
    ui_draw_fill_rect(&hud, HUD_BG_COLOR);
    ui_render_set_clip(&game_area);
    tb_draw_hud();
}
ui_render_reset_clip();
```

**关键原则**：
- 每个脏区域内的擦除和球重绘必须**连续完成**，不要先擦完所有再统一重绘
- 擦除时 clip 设为精确脏区域（只擦必要的部分）
- 重绘时 clip 设为游戏区域（防止画到标题栏，同时允许球完整绘制）
- HUD 等固定 UI 元素**不要在循环内多次擦除重绘**，应收集是否需要重绘的标志，循环结束后统一擦除+重绘一次

**坑**：如果重绘时不设置 clip 或设为全屏，球飞到标题栏区域时会覆盖标题栏。

---

## 3. Widget 绘制

### 游戏页面绕过了 `ui_page_draw`

**坑**：游戏页面的 `on_draw` 直接被 `UI_Tick` 调用，不会经过 `ui_page_draw`，因此 widgets（返回按钮、标题文字）不会被自动绘制。

解决方案：在全屏重绘时手动绘制 widgets：

```c
if (s_tb.need_full_redraw) {
    // 画标题栏背景
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    // 手动绘制 widgets
    ui_rect_t full = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
    for (uint16_t j = 0; j < page->widget_count; j++) {
        if (page->widgets[j]) {
            ui_widget_draw(page->widgets[j], &full);
        }
    }
    // ...
}
```

### 页面退出时的残留问题

**坑**：游戏页面直接绘制到屏幕（绕过 `ui_page_draw`），返回上一页时，游戏绘制的内容可能残留在屏幕上。

解决方案：`ui_page_pop` 中调用 `ui_page_invalidate_all()` 标记全屏脏区域，下一帧 `ui_page_draw` 会完整重绘 sidebar 和内容区域，覆盖游戏残留内容。

**坑**：不要使用 `ui_screen_clear` 清屏——它会用白色填充整个屏幕（包括 sidebar 区域），导致 sidebar 文字被覆盖。`ui_page_invalidate_all()` 已足够触发完整重绘。

**坑**：游戏 `on_draw` 中如果调用了 `ui_render_set_clip`，必须在函数结束前调用 `ui_render_reset_clip()`。否则 clip 状态残留，下一帧 `ui_page_draw` 绘制 sidebar 和标题栏时会被裁剪掉，导致残留内容无法被覆盖。

---

## 4. 触摸事件

### 多点触控支持

MiniUI 框架支持最多 5 点同时触控（`UI_MAX_TOUCH_POINTS = 5`）。触摸驱动（GT911）会为每个触点分配唯一的 track_id，并通过 `ui_input_touch_multi_raw` 生成独立的多点触控事件。

**多点触控事件类型**：
```c
UI_EVENT_TOUCH_DOWN   // 新手指按下，e->touch_id 为触点编号 (0-4)
UI_EVENT_TOUCH_MOVE   // 手指移动
UI_EVENT_TOUCH_UP     // 手指抬起
```

**与旧事件的关系**：
- `ui_input_touch_raw` 仍然生成旧事件（`UI_EVENT_PRESS/RELEASE/DRAG`），只跟踪第一个触点
- `ui_input_touch_multi_raw` 生成新的多点触控事件，跟踪所有触点
- 旧事件用于兼容不需要多点触控的游戏和 UI，新事件用于需要同时按多个按钮的游戏

**多点触控事件分发**：
- 多点触控事件**绕过 capture 机制**，广播给页面的所有 widget
- 这确保多个手指可以同时操作不同的 UI 元素（如同时按方向键和跳跃键）
- 旧的 `UI_EVENT_PRESS` 仍会设置 capture，但不影响多点触控事件

### 需要多点触控的游戏实现模式

对于需要同时按多个按钮的游戏（如 Contra），使用以下模式：

```c
/* 每个触点跟踪它按下的按钮（位掩码） */
static uint8_t s_touch_btns[UI_MAX_TOUCH_POINTS];

/* 检测屏幕坐标对应哪个按钮 */
#define BTN_LEFT   (1 << 0)
#define BTN_RIGHT  (1 << 1)
#define BTN_JUMP   (1 << 2)
#define BTN_SHOOT  (1 << 3)

static uint8_t hit_test(int16_t x, int16_t y) {
    uint8_t mask = 0;
    if (in_dpad_left(x, y))  mask |= BTN_LEFT;
    if (in_dpad_right(x, y)) mask |= BTN_RIGHT;
    if (in_btn_jump(x, y))   mask |= BTN_JUMP;
    if (in_btn_shoot(x, y))  mask |= BTN_SHOOT;
    return mask;
}

/* 从所有触点重新计算输入状态 */
static void recalc_inputs(void) {
    uint8_t combined = 0;
    for (int i = 0; i < UI_MAX_TOUCH_POINTS; i++)
        combined |= s_touch_btns[i];
    s_game.input_left  = (combined & BTN_LEFT)  != 0;
    s_game.input_right = (combined & BTN_RIGHT) != 0;
    s_game.input_jump  = (combined & BTN_JUMP)  != 0;
    s_game.input_shoot = (combined & BTN_SHOOT) != 0;
}

/* 事件处理 */
static void touch_event(ui_widget_t *w, ui_event_t *e) {
    if (e->type == UI_EVENT_TOUCH_DOWN) {
        s_touch_btns[e->touch_id] = hit_test(e->pos.x, e->pos.y);
        recalc_inputs();
    } else if (e->type == UI_EVENT_TOUCH_MOVE) {
        s_touch_btns[e->touch_id] = hit_test(e->pos.x, e->pos.y);
        recalc_inputs();
    } else if (e->type == UI_EVENT_TOUCH_UP) {
        s_touch_btns[e->touch_id] = 0;
        recalc_inputs();
    }
}
```

**关键点**：
- 每个触点独立跟踪它按下的按钮
- 一个触点抬起时，只清除该触点的按钮，不影响其他触点按着的按钮
- `recalc_inputs` 用 OR 合并所有触点的按钮状态，确保只要有一个触点按着某按钮，该按钮就保持激活

### 去抖时间不能太长

**坑**：`TOUCH_DEBOUNCE_FRAMES = 10` 配合 `TOUCH_SCAN_DIVIDER = 2`，意味着释放后约 333ms 内无法识别新的按下。快速点击时第二次触摸被识别为 DRAG 而非 PRESS，导致无法点击游戏中的小球。

建议：`TOUCH_DEBOUNCE_FRAMES = 2`（约 67ms），足以消除抖动又不影响快速点击。

### 触摸区域 widget

游戏需要全屏触摸区域来捕获点击：

```c
ui_rect_t touch_rect = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                        UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
ui_widget_init(&s_touch_area, &touch_rect);
s_touch_area.bg_color = UI_COLOR_TRANSPARENT;
s_touch_area.event_cb = tb_touch_event;
```

Widget 顺序影响事件分发：高 index 的 widget 先接收事件。返回按钮应在最高 index，确保它能优先处理点击。

---

## 5. 帧率控制与防撕裂

### VSYNC 同步（防撕裂）

SSD1963 是单帧缓冲架构，LCD 面板持续从帧缓冲读取数据驱动屏幕。如果 MCU 在 LCD 扫描期间写入帧缓冲，会导致画面撕裂（上半帧是旧数据，下半帧是新数据）。

解决方案：使用 SSD1963 的 TE（Tearing Effect）信号同步写入时机。

**硬件连接**：SSD1963 TE 引脚 → CH32H417 PB8

**配置步骤**：

1. SSD1963 初始化时使能 TE 输出：
```c
SSD1963_WriteCmd(SSD1963_SET_TEAR_ON);  // 0x35
SSD1963_WriteData(0x00);  // bit0=0: 仅 V-blanking 时输出 TE
```

2. 配置 PB8 为 GPIO 输入：
```c
SSD1963_TE_Init();
```

3. 主循环中每帧绘制前等待 VSYNC：
```c
while(1) {
    SSD1963_WaitVSync();  // 等待 TE 上升沿（V-blanking 开始）
    Touch_Scan();
    UI_Tick();
    // ... 帧率限制 ...
}
```

**原理**：TE 信号在垂直消隐期（V-blanking）开始时拉高，此时 LCD 面板不在扫描有效数据，MCU 可以安全写入帧缓冲。消隐期持续时间 = (VT - VDP) 行 × 行时间，约 2-3ms，足够完成局部刷新。

### 主循环帧率限制

在 VSYNC 同步基础上，使用 `__get_UCYCLE()` 测量帧时间，延时到目标帧率：

```c
uint32_t cycles_per_ms = SystemCoreClock / 1000;
uint32_t frame_cycles = cycles_per_ms * 40;  // 40ms ≈ 25fps
uint32_t last_frame = __get_UCYCLE();

while(1) {
    SSD1963_WaitVSync();
    Touch_Scan();
    UI_Tick();

    uint32_t now = __get_UCYCLE();
    uint32_t elapsed = now - last_frame;
    if (elapsed < frame_cycles) {
        uint32_t remaining_ms = (frame_cycles - elapsed) / cycles_per_ms;
        if (remaining_ms > 0) Delay_Ms(remaining_ms);
    } else {
        Delay_Ms(1);  // 最小延时
    }
    last_frame = __get_UCYCLE();
}
```

---

## 6. DMA 与 FMC 写入

### DMA 写入 FMC NORSRAM 不可靠

**坑**：CH32H417 的 FMC NORSRAM 控制器没有写 FIFO，DMA 写入速度（7 cyc/px）远快于 CPU 写入（28 cyc/px），但会导致 SSD1963 数据丢失，显示异常（背景无法绘制、残影）。

尝试过的方案及结果：
- 16位 DMA：7 cyc/px 但显示异常
- 256位突发 DMA：因无写 FIFO，反而更慢（83ms vs 27ms）
- FMC WriteBurst Enable + DMA 同步读：仍然异常
- DataSetupTime=0：CPU 模式也不稳定

结论：在当前硬件配置下，CPU 写入 + 局部刷新是更可靠的方案。

### CPU 写入优化

8x 循环展开对 `SSD1963_FillColor` 无明显提升（编译器已做类似优化），但代码保留也无害。

---

## 7. MiniUI Dirty Rect API 参考

```c
// 标记单个区域为脏
void ui_page_invalidate(const ui_rect_t *rect);

// 标记全屏为脏
void ui_page_invalidate_all(void);

// 获取当前脏区域列表（只读）
const ui_dirty_list_t *ui_page_get_dirty_list(void);

// 清除脏区域列表
void ui_page_clear_dirty(void);

// 设置/重置渲染裁剪区域
void ui_render_set_clip(const ui_rect_t *rect);
void ui_render_reset_clip(void);
```

脏区域列表最多 8 个区域（`UI_MAX_DIRTY_REGIONS`），重叠区域会自动合并。超出上限时退化为全屏脏区域。

---

## 8. 防闪烁经验总结

SSD1963 是单帧缓冲架构，LCD 面板以 60Hz 持续从帧缓冲读取数据驱动屏幕。MCU 写入和 LCD 扫描同时进行，如果处理不当会产生闪烁和撕裂。以下是踩过的坑和解决方案：

### 8.1 撕裂（Tearing）

**现象**：画面从上到下出现错位，上半帧是旧数据，下半帧是新数据。

**原因**：MCU 在 LCD 扫描期间写入帧缓冲，LCD 扫描到写入位置时看到的是半新半旧的数据。

**解决方案**：使用 SSD1963 TE 信号同步写入时机（见第 5 章 VSYNC 同步）。

### 8.2 闪烁（Flickering）

**现象**：移动的物体（球、HUD 文字）在移动时一闪一闪。

**原因**：擦除旧位置和绘制新位置之间有时间间隔，LCD 扫描到了擦除后的空白帧。

**解决方案**：

1. **单遍原子绘制**：每个脏区域内擦除后立即重绘，不要先擦完所有再统一重绘。这是最关键的措施。

2. **VSYNC 同步**：在消隐期内完成所有写入，LCD 永远不会扫描到中间状态。

3. **HUD 等固定 UI 只擦除重绘一次**：球飞过 HUD 区域时，擦除球会连带擦掉 HUD 文字。如果每个脏区域重叠 HUD 都擦除+重绘一次，一帧内 HUD 可能被擦除重绘多次，闪烁概率翻倍。正确做法：循环内只记录 HUD 是否需要重绘，循环结束后统一擦除+重绘一次：
```c
bool hud_needs_redraw = false;
for (each dirty rect) {
    // ... 擦除 + 画球 ...
    if (rects_overlap(&clip, &hud)) hud_needs_redraw = true;
}
if (hud_needs_redraw) {
    ui_render_set_clip(&hud);
    ui_draw_fill_rect(&hud, HUD_BG_COLOR);  // 先擦
    ui_render_set_clip(&game_area);
    tb_draw_hud();                            // 后画
}
```

### 8.3 残影（Ghosting）

**现象**：物体移动后旧位置仍有残影。

**原因**：脏区域标记不够大，没有覆盖物体的完整范围。

**解决方案**：标记脏区域时，确保包含物体的完整边界（包括圆角、阴影等），宁可多标一些也不要少标。

**常见残影场景及修复**：

1. **实体被消灭时**：实体被消灭（`active = false`）后，`prev_x/prev_y` 是上一帧的位置，但实体在当前帧移动了，碰撞位置（`x/y`）与上一帧位置不同。需要同时标记 `prev` 位置和当前位置：
```c
if (!e->active && (e->prev_x != 0 || e->prev_y != 0)) {
    // 标记上一帧位置
    inv_world_rect(e->prev_x, e->prev_y, w, h, prev_camera);
    // 标记碰撞位置（当前位置）
    inv_world_rect(e->x, e->y, w, h, camera);
}
```

2. **无敌闪烁时**：玩家被击中后进入无敌状态，每帧闪烁（可见/不可见交替）。即使位置没变，可见性变化也需要重绘：
```c
if (s_game.invincible > 0) {
    inv_player(s_game.px, s_game.py, s_game.camera_x);
}
```

3. **提前 return 导致脏区域未标记**：在碰撞检测中调用 `player_die()` 后直接 `return`，跳过了末尾的脏区域标记代码。必须在 `return` 前手动标记旧位置：
```c
if (bullet_hit_player) {
    inv_bullet(b, camera);  // 标记子弹消失位置
    player_die();           // 内部标记玩家旧位置和新位置
    return;                 // 跳过后续脏区域标记
}
```

### 8.4 相机移动时的局刷策略

**核心原则**：相机移动时不要全刷，而是标记所有移动元素的旧位置和新位置。

当相机移动时，所有世界坐标的物体在屏幕上的位置都会改变。但背景（天空）是均匀的，擦除脏区域后用天空色填充即可正确恢复。

**标记策略**：

1. **边缘条**：相机滚动时新出现的屏幕边缘区域
2. **平台**：在旧相机偏移和新相机偏移下各标记一次
3. **视差背景（山脉等）**：在旧偏移和新偏移下各标记一次
4. **所有实体**：旧位置（旧相机偏移）+ 新位置（新相机偏移）
5. **HUD**：仅在数据变化时标记

```c
if (camera_x != prev_camera_x) {
    // 1. 边缘条
    int16_t dx = camera_x - prev_camera_x;
    if (dx > 0) inv_rect(SCREEN_W - dx, AREA_Y, dx, AREA_H);
    else        inv_rect(0, AREA_Y, -dx, AREA_H);

    // 2. 平台（旧+新偏移）
    for (each platform p) {
        inv_world_rect_at(p->x, p->y, p->w, p->h, prev_camera_x);
        inv_world_rect_at(p->x, p->y, p->w, p->h, camera_x);
    }

    // 3. 视差背景（旧+新偏移）
    // ... 根据具体视差系数计算 ...

    // 4. 实体旧位置 + 新位置
    inv_player(prev_px, prev_py, prev_camera_x);
    inv_player(px, py, camera_x);
    // ... enemies, bullets, particles 同理 ...

    // 5. HUD 仅在数据变化时
    if (hud_dirty) inv_hud();

    return;  // 跳过常规脏区域标记
}
```

**不要标记的内容**：
- 标题栏（不受相机移动影响）
- 控制按钮（不受相机移动影响，除非输入状态变化）
- 静态 UI 元素

**脏区域裁剪**：`inv_world_rect_at` 应将坐标裁剪到游戏区域内，避免脏区域扩展到标题栏：
```c
static void inv_world_rect_at(int16_t wx, int16_t wy, int16_t w, int16_t h, int16_t cam_x) {
    int16_t sx = wx - cam_x;
    int16_t sy = wy + AREA_Y;
    if (sx + w <= 0 || sx >= AREA_W) return;
    if (sy + h <= AREA_Y || sy >= SCREEN_H) return;
    if (sx < 0) { w += sx; sx = 0; }
    if (sy < AREA_Y) { h -= (AREA_Y - sy); sy = AREA_Y; }
    ui_rect_t r = {sx, sy, w, h};
    ui_page_invalidate(&r);
}
```

**局刷绘制时需包含背景元素**：`draw_entities_in_clip` 必须绘制所有可能与脏区域重叠的元素，包括视差背景（山脉）、装饰（星星）、平台等，否则这些元素在脏区域内会消失。

### 8.5 clip 残留

**坑**：`on_draw` 中调用了 `ui_render_set_clip` 但忘记在函数末尾调用 `ui_render_reset_clip`，导致下一帧 `ui_page_draw` 绘制 sidebar 和标题栏时被裁剪掉，返回上一页后出现残留。

**解决方案**：`on_draw` 函数末尾必须调用 `ui_render_reset_clip()`。

### 8.6 VSYNC 等待位置

**坑**：`SSD1963_WaitVSync()` 放在 `Touch_Scan()` 之前，Touch_Scan 的 I2C 通信耗时 1-2ms，等开始绘制时消隐期已经过了，VSYNC 同步失效。

**解决方案**：先 `Touch_Scan()`，再 `SSD1963_WaitVSync()`，然后立即 `UI_Tick()` 绘制：
```c
while(1) {
    Touch_Scan();          // I2C 通信，不写 GRAM
    SSD1963_WaitVSync();   // 等待消隐期开始
    UI_Tick();             // 立即绘制，在消隐期内完成
    // ... 帧率限制 ...
}
```

### 8.7 TE 信号超时保护

**坑**：如果 TE 引脚未连接或 SSD1963 未正确配置，`SSD1963_WaitVSync()` 会死等。

**解决方案**：加超时退出：
```c
void SSD1963_WaitVSync(void)
{
    uint32_t timeout = 100000;  // ~1ms at 100MHz
    while (SSD1963_TE_IsHigh()) { if (--timeout == 0) return; }
    timeout = 100000;
    while (!SSD1963_TE_IsHigh()) { if (--timeout == 0) return; }
}
```
