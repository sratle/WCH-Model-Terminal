#include "touch_driver.h"
#include "gt911.h"
#include "../MiniUI/miniui.h"
#include "debug.h"

#define TOUCH_X_RESOLUTION  800
#define TOUCH_Y_RESOLUTION  480
#define TOUCH_MAX_POINTS    5
#define TOUCH_DEBOUNCE_FRAMES  2
#define TOUCH_SCAN_DIVIDER      1       /* Scan touch every N frames */

static bool s_touch_initialized = false;
static bool s_last_pressed = false;
static Touch_Point_t s_last_point = {0, 0, false};
static uint8_t s_debounce_counter = 0;
static uint8_t s_scan_counter = 0;

/* Multi-touch tracking: map GT911 track_id to our touch slot index */
static bool s_active_ids[TOUCH_MAX_POINTS];
static GT911_TouchPoint_t s_active_pos[TOUCH_MAX_POINTS];

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
    memset(s_active_ids, 0, sizeof(s_active_ids));
}

/* Find slot index for a given GT911 track_id, or -1 if not found */
static int8_t find_slot_by_track_id(uint8_t track_id)
{
    for (uint8_t i = 0; i < TOUCH_MAX_POINTS; i++) {
        if (s_active_ids[i] && s_active_pos[i].track_id == track_id)
            return (int8_t)i;
    }
    return -1;
}

/* Find a free slot index, or -1 if all full */
static int8_t find_free_slot(void)
{
    for (uint8_t i = 0; i < TOUCH_MAX_POINTS; i++) {
        if (!s_active_ids[i])
            return (int8_t)i;
    }
    return -1;
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
        /* Legacy single-touch for backward compatibility */
        s_last_point.x = points[0].x;
        s_last_point.y = points[0].y;
        s_last_point.pressed = true;
        s_debounce_counter = 0;
        ui_input_touch_raw(true, points[0].x, points[0].y);

        /* Multi-touch: match current points to existing slots by track_id */
        bool matched[TOUCH_MAX_POINTS] = {false};

        /* First pass: match existing track_ids (finger moved) */
        for (uint8_t i = 0; i < touch_count; i++) {
            int8_t slot = find_slot_by_track_id(points[i].track_id);
            if (slot >= 0) {
                /* Existing finger moved */
                matched[slot] = true;
                s_active_pos[slot] = points[i];
                ui_input_touch_multi_raw((uint8_t)slot, true, points[i].x, points[i].y);
            }
        }

        /* Second pass: assign new track_ids to free slots (new finger) */
        for (uint8_t i = 0; i < touch_count; i++) {
            int8_t existing = find_slot_by_track_id(points[i].track_id);
            if (existing >= 0) continue; /* Already matched */

            int8_t slot = find_free_slot();
            if (slot < 0) continue; /* No free slots */

            s_active_ids[slot] = true;
            s_active_pos[slot] = points[i];
            matched[slot] = true;
            ui_input_touch_multi_raw((uint8_t)slot, true, points[i].x, points[i].y);
        }

        /* Release slots that are no longer present */
        for (uint8_t i = 0; i < TOUCH_MAX_POINTS; i++) {
            if (s_active_ids[i] && !matched[i]) {
                ui_input_touch_multi_raw(i, false, s_active_pos[i].x, s_active_pos[i].y);
                s_active_ids[i] = false;
            }
        }
    } else {
        if (s_last_pressed) {
            s_debounce_counter++;
            if (s_debounce_counter >= TOUCH_DEBOUNCE_FRAMES) {
                ui_input_touch_raw(false, s_last_point.x, s_last_point.y);
                s_last_point.pressed = false;
                s_debounce_counter = 0;

                /* Multi-touch: release all previously active touch points */
                for (uint8_t i = 0; i < TOUCH_MAX_POINTS; i++) {
                    if (s_active_ids[i]) {
                        ui_input_touch_multi_raw(i, false, s_active_pos[i].x, s_active_pos[i].y);
                        s_active_ids[i] = false;
                    }
                }
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
