#include "touch_driver.h"
#include "gt911.h"
#include "../MiniUI/miniui.h"
#include "debug.h"

#define TOUCH_X_RESOLUTION  800
#define TOUCH_Y_RESOLUTION  480
#define TOUCH_MAX_POINTS    5

static bool s_touch_initialized = false;
static bool s_last_pressed = false;
static Touch_Point_t s_last_point = {0, 0, false};

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

    GT911_TouchPoint_t points[TOUCH_MAX_POINTS];
    uint8_t touch_count = 0;

    GT911_Status_t result = GT911_ReadTouch(points, &touch_count);
    if (result != GT911_OK) return;

    if (touch_count > 0) {
        s_last_point.x = points[0].x;
        s_last_point.y = points[0].y;
        s_last_point.pressed = true;
        // printf("[Touch] PRESS (%d, %d)\r\n", points[0].x, points[0].y);
        ui_input_touch_raw(true, points[0].x, points[0].y);
    } else {
        if (s_last_pressed) {
            // printf("[Touch] RELEASE (%d, %d)\r\n", s_last_point.x, s_last_point.y);
            ui_input_touch_raw(false, s_last_point.x, s_last_point.y);
        }
        s_last_point.pressed = false;
    }

    s_last_pressed = (touch_count > 0);
}

bool Touch_GetPoint(Touch_Point_t *point)
{
    if (!s_touch_initialized || !s_last_point.pressed) return false;
    *point = s_last_point;
    return true;
}
