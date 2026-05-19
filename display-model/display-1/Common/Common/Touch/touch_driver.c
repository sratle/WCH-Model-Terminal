#include "touch_driver.h"
#include "gt911.h"
#include "../MiniUI/miniui.h"
#include "debug.h"

#define TOUCH_X_RESOLUTION  800
#define TOUCH_Y_RESOLUTION  480
#define TOUCH_MAX_POINTS    5
#define TOUCH_DEBOUNCE_FRAMES  10
#define TOUCH_SCAN_DIVIDER      2       /* Scan touch every N frames */

static bool s_touch_initialized = false;
static bool s_last_pressed = false;
static Touch_Point_t s_last_point = {0, 0, false};
static uint8_t s_debounce_counter = 0;
static uint8_t s_scan_counter = 0;

void Touch_Init(void)
{
    GT911_Config_t config = {
        .X_Resolution = TOUCH_X_RESOLUTION,
        .Y_Resolution = TOUCH_Y_RESOLUTION,
        .Number_Of_Touch_Support = TOUCH_MAX_POINTS,
        .ReverseX = false,
        .ReverseY = false,
        .SwithX2Y = false,
        .SoftwareNoiseReduction = false,
    };

    GT911_Status_t result = GT911_Init(config);
    if (result != GT911_OK) {
        printf("[Touch] GT911_Init failed: %d\r\n", result);
        s_touch_initialized = false;
        return;
    }

    s_touch_initialized = true;
}

void Touch_Scan(void)
{
    if (!s_touch_initialized) return;

    /* Only perform I2C read every TOUCH_SCAN_DIVIDER frames to reduce overhead */
    s_scan_counter++;
    if (s_scan_counter < TOUCH_SCAN_DIVIDER) return;
    s_scan_counter = 0;

    GT911_TouchPoint_t points[TOUCH_MAX_POINTS];
    uint8_t touch_count = 0;

    GT911_Status_t result = GT911_ReadTouch(points, &touch_count);
    if (result != GT911_OK) return;

    if (touch_count > 0) {
        s_last_point.x = points[0].x;
        s_last_point.y = points[0].y;
        s_last_point.pressed = true;
        s_debounce_counter = 0;
        ui_input_touch_raw(true, points[0].x, points[0].y);
    } else {
        if (s_last_pressed) {
            s_debounce_counter++;
            if (s_debounce_counter >= TOUCH_DEBOUNCE_FRAMES) {
                ui_input_touch_raw(false, s_last_point.x, s_last_point.y);
                s_last_point.pressed = false;
                s_debounce_counter = 0;
            }
        }
    }

    s_last_pressed = (touch_count > 0) || (s_debounce_counter > 0);
}

bool Touch_GetPoint(Touch_Point_t *point)
{
    if (!s_touch_initialized || !s_last_point.pressed) return false;
    *point = s_last_point;
    return true;
}
