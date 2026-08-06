/********************************** (C) COPYRIGHT *******************************
* File Name          : app_lrange.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2026/08/06
* Description        : Laser ranging app (VL53L0X, submodel-6).
*                      Real-time distance display with proximity warning
*                      (user closer than 10cm) and a distance line chart.
*                      Data received via DISP_EXT_SUBMODEL_EVENT from Core.
*                      History stored only while app is active (saves RAM).
********************************************************************************/
#include "app_lrange.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 *  Layout Configuration (800x480)
 *=============================================================================*/

#define LR_SCREEN_W             UI_SCREEN_WIDTH
#define LR_SCREEN_H             UI_SCREEN_HEIGHT
#define LR_CONTENT_Y            (APP_TITLE_BAR_H + 4)
#define LR_CONTENT_H            (LR_SCREEN_H - APP_TITLE_BAR_H - 4)

/* Value card */
#define LR_CARD_X               8
#define LR_CARD_Y               LR_CONTENT_Y
#define LR_CARD_W               (LR_SCREEN_W - 16)
#define LR_CARD_H               90
#define LR_CARD_R               8

/* Proximity warning banner */
#define LR_WARN_Y               (LR_CARD_Y + LR_CARD_H + 8)
#define LR_WARN_H               44

/* Chart area */
#define LR_CHART_X              8
#define LR_CHART_W              (LR_SCREEN_W - 16)
#define LR_CHART_Y              (LR_WARN_Y + LR_WARN_H + 8)
#define LR_CHART_H              180
#define LR_CHART_PAD_X          4
#define LR_CHART_PAD_Y          4

/* Stats area */
#define LR_STATS_Y              (LR_CHART_Y + LR_CHART_H + 8)
#define LR_STATS_H              60

/* Bottom status bar */
#define LR_CTRL_Y               (LR_SCREEN_H - 44)
#define LR_CTRL_H               40

/*=============================================================================
 *  Ranging Constants
 *=============================================================================*/

#define LR_PROXIMITY_MM         100     /* 10cm: user too close to screen */
#define LR_CHART_Y_MAX          1300    /* chart Y axis max (default mode 1200mm) */
#define LR_HISTORY_LEN          60

/*=============================================================================
 *  History Buffer (16-bit distance values)
 *=============================================================================*/

typedef struct {
    uint16_t data[LR_HISTORY_LEN];
    uint16_t count;
    uint16_t write_idx;
    uint32_t sum;
    uint8_t  sum_count;
} lr_history_t;

static void lr_history_push(lr_history_t *h, uint16_t val)
{
    h->data[h->write_idx] = val;
    h->write_idx = (h->write_idx + 1) % LR_HISTORY_LEN;
    if (h->count < LR_HISTORY_LEN)
        h->count++;
    if (h->sum_count < 255) {
        h->sum += val;
        h->sum_count++;
    } else {
        uint32_t s = 0;
        uint16_t n = (h->count < LR_HISTORY_LEN) ? h->count : LR_HISTORY_LEN;
        for (uint16_t i = 0; i < n; i++)
            s += h->data[i];
        h->sum = s;
        h->sum_count = (uint8_t)n;
    }
}

static uint16_t lr_history_avg(const lr_history_t *h)
{
    if (h->sum_count == 0) return 0;
    return (uint16_t)(h->sum / h->sum_count);
}

static uint16_t lr_history_min(const lr_history_t *h)
{
    if (h->count == 0) return 0;
    uint16_t n = (h->count < LR_HISTORY_LEN) ? h->count : LR_HISTORY_LEN;
    uint16_t mn = 0xFFFF;
    for (uint16_t i = 0; i < n; i++) {
        if (h->data[i] < mn) mn = h->data[i];
    }
    return mn;
}

static uint16_t lr_history_max(const lr_history_t *h)
{
    if (h->count == 0) return 0;
    uint16_t n = (h->count < LR_HISTORY_LEN) ? h->count : LR_HISTORY_LEN;
    uint16_t mx = 0;
    for (uint16_t i = 0; i < n; i++) {
        if (h->data[i] > mx) mx = h->data[i];
    }
    return mx;
}

/*=============================================================================
 *  Colors
 *=============================================================================*/

#define LR_BG                   UI_COLOR_BG_MAIN
#define LR_CARD_BG              UI_COLOR_BG_CARD
#define LR_CARD_BORDER          UI_HEX(0xE0E0E0)
#define LR_TEXT                 UI_COLOR_TEXT_PRIMARY
#define LR_TEXT_DIM             UI_COLOR_TEXT_SECONDARY
#define LR_CHART_BG             UI_HEX(0xFAFAFA)
#define LR_CHART_GRID           UI_HEX(0xE8E8E8)
#define LR_CHART_BORDER         UI_HEX(0xD0D0D0)

#define LR_OK_COLOR             UI_HEX(0x1E88E5)   /* Blue */
#define LR_OK_BG                UI_HEX(0xE3F2FD)
#define LR_WARN_COLOR           UI_HEX(0xE53935)   /* Red */
#define LR_WARN_BG              UI_HEX(0xFFEBEE)
#define LR_CHART_FILL           UI_HEX(0xD0E8FC)
#define LR_THRESHOLD_COLOR      UI_HEX(0xEF9A9A)

/*=============================================================================
 *  State
 *=============================================================================*/

static ui_app_page_t s_app_lrange;

static uint16_t    s_distance_mm;   /* Latest averaged distance */
static bool        s_data_valid;    /* Have received at least one result */
static bool        s_out_of_range;  /* Last report was a failure */
static uint8_t     s_last_err;      /* Last failure error code */

static lr_history_t s_dist_hist;

/* Widgets */
static ui_widget_t s_touch_area;
static ui_widget_t *s_lrange_widgets[3]; /* back + title + touch */

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void lr_on_submodel_event(uint8_t sub_type, uint8_t sub_cmd,
                                 const uint8_t *evt_data, uint8_t evt_len);

static uart_submodel_cb_t s_lrange_submodel_cb = {
    .on_submodel_event = lr_on_submodel_event,
};

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void lr_invalidate_content(void)
{
    ui_rect_t r = {0, LR_CONTENT_Y, LR_SCREEN_W, LR_CONTENT_H};
    ui_page_invalidate(&r);
}

static bool lr_is_too_close(void)
{
    return s_data_valid && !s_out_of_range && s_distance_mm > 0 &&
           s_distance_mm < LR_PROXIMITY_MM;
}

/*=============================================================================
 *  Submodel Event Handler
 *=============================================================================*/

static void lr_on_submodel_event(uint8_t sub_type, uint8_t sub_cmd,
                                 const uint8_t *evt_data, uint8_t evt_len)
{
    if (sub_type != SUBMODEL_LASER)
        return;

    switch (sub_cmd) {
    case LR_EVT_RESULT_OK:
    {
        if (!evt_data || evt_len < 2)
            return;

        uint16_t dist = ((uint16_t)evt_data[0] << 8) | evt_data[1];

        s_distance_mm = dist;
        s_data_valid = true;
        s_out_of_range = false;

        lr_history_push(&s_dist_hist, dist);
        lr_invalidate_content();
        break;
    }
    case LR_EVT_RESULT_FAIL:
    {
        if (!evt_data || evt_len < 1)
            return;

        s_last_err = evt_data[0];
        s_out_of_range = true;
        lr_invalidate_content();
        break;
    }
    default:
        break;
    }
}

/*=============================================================================
 *  Chart Drawing
 *=============================================================================*/

static void lr_draw_chart(const ui_rect_t *rect, const lr_history_t *hist,
                          uint16_t y_min, uint16_t y_max, ui_color_t line_color)
{
    int16_t x = rect->x + LR_CHART_PAD_X;
    int16_t y = rect->y + LR_CHART_PAD_Y;
    int16_t w = rect->w - 2 * LR_CHART_PAD_X;
    int16_t h = rect->h - 2 * LR_CHART_PAD_Y;

    /* Background */
    ui_draw_fill_rect(rect, LR_CHART_BG);
    ui_draw_rect_border(rect, LR_CHART_BORDER, 1);

    /* Grid lines (3 horizontal) */
    for (int i = 1; i <= 3; i++) {
        int16_t gy = y + h * i / 4;
        ui_draw_hline(x, gy, w, LR_CHART_GRID);
    }

    /* 10cm proximity threshold line */
    {
        uint16_t range = y_max - y_min;
        int16_t ty = y + h - (int16_t)((uint32_t)(LR_PROXIMITY_MM - y_min) * h / range);
        if (ty >= y && ty <= y + h) {
            for (int16_t dx = 0; dx < w; dx += 8)
                ui_draw_hline(x + dx, ty, 4, LR_THRESHOLD_COLOR);
        }
    }

    uint16_t n = (hist->count < LR_HISTORY_LEN) ? hist->count : LR_HISTORY_LEN;
    if (n < 2) {
        ui_draw_text(rect->x + rect->w / 2 - 30, rect->y + rect->h / 2 - 6,
                     "No data", UI_FONT_BODY, LR_TEXT_DIM);
        return;
    }

    uint16_t range = y_max - y_min;
    if (range == 0) range = 1;

    /* Fill area under curve */
    for (uint16_t i = 0; i < n; i++) {
        uint16_t idx;
        if (hist->count >= LR_HISTORY_LEN)
            idx = (hist->write_idx + i) % LR_HISTORY_LEN;
        else
            idx = i;

        uint16_t val = hist->data[idx];
        if (val > y_max) val = y_max;
        int16_t px = x + (int16_t)((uint32_t)w * i / (n - 1));
        int16_t py = y + h - (int16_t)((uint32_t)(val - y_min) * h / range);
        if (py < y) py = y;
        if (py > y + h) py = y + h;

        if (py < y + h - 1) {
            ui_draw_vline(px, py, y + h - py, LR_CHART_FILL);
        }
    }

    /* Draw line segments */
    int16_t prev_px = 0, prev_py = 0;
    for (uint16_t i = 0; i < n; i++) {
        uint16_t idx;
        if (hist->count >= LR_HISTORY_LEN)
            idx = (hist->write_idx + i) % LR_HISTORY_LEN;
        else
            idx = i;

        uint16_t val = hist->data[idx];
        if (val > y_max) val = y_max;
        int16_t px = x + (int16_t)((uint32_t)w * i / (n - 1));
        int16_t py = y + h - (int16_t)((uint32_t)(val - y_min) * h / range);
        if (py < y) py = y;
        if (py > y + h) py = y + h;

        if (i > 0) {
            ui_draw_line(prev_px, prev_py, px, py, line_color);
        }
        prev_px = px;
        prev_py = py;
    }
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void lr_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetSubmodelCallbacks(&s_lrange_submodel_cb);

    s_data_valid = false;
    s_out_of_range = false;
    s_distance_mm = 0;
    s_last_err = 0;
    memset(&s_dist_hist, 0, sizeof(s_dist_hist));
}

static void lr_page_exit(ui_page_t *page)
{
    (void)page;
    UART_ClearSubmodelCallbacks();
}

static void lr_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;

    /* Title bar background */
    ui_rect_t bar = {0, 0, LR_SCREEN_W, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    int16_t dirty_top = dirty->y;
    int16_t dirty_bot = dirty->y + dirty->h;

    bool too_close = lr_is_too_close();

    /* ---- Value card ---- */
    if (dirty_top < LR_CARD_Y + LR_CARD_H && dirty_bot > LR_CARD_Y) {
        ui_rect_t card = {LR_CARD_X, LR_CARD_Y, LR_CARD_W, LR_CARD_H};
        ui_color_t accent = too_close ? LR_WARN_COLOR : LR_OK_COLOR;
        ui_color_t card_bg = too_close ? LR_WARN_BG : LR_OK_BG;

        ui_draw_fill_round_rect(&card, LR_CARD_R, card_bg);
        ui_draw_round_rect_border(&card, LR_CARD_R, accent, 2);

        ui_draw_text(LR_CARD_X + 12, LR_CARD_Y + 8, "Distance",
                     UI_FONT_BODY, accent);

        if (s_data_valid && !s_out_of_range) {
            char val_str[12];
            snprintf(val_str, sizeof(val_str), "%d", s_distance_mm);
            ui_draw_text(LR_CARD_X + 12, LR_CARD_Y + 28, val_str,
                         UI_FONT_TITLE, LR_TEXT);
            ui_draw_text(LR_CARD_X + 12 + ui_text_width(val_str, UI_FONT_TITLE) + 6,
                         LR_CARD_Y + 36, "mm",
                         UI_FONT_BODY, LR_TEXT_DIM);

            if (s_dist_hist.count > 0) {
                char avg_str[24];
                snprintf(avg_str, sizeof(avg_str), "Avg: %d mm",
                         lr_history_avg(&s_dist_hist));
                ui_draw_text(LR_CARD_X + 12, LR_CARD_Y + 62, avg_str,
                             UI_FONT_BODY, LR_TEXT_DIM);
            }
        } else if (s_out_of_range) {
            ui_draw_text(LR_CARD_X + 12, LR_CARD_Y + 32, "Out of range",
                         UI_FONT_TITLE, LR_WARN_COLOR);
            char err_str[20];
            snprintf(err_str, sizeof(err_str), "err=%d", s_last_err);
            ui_draw_text(LR_CARD_X + 12, LR_CARD_Y + 62, err_str,
                         UI_FONT_BODY, LR_TEXT_DIM);
        } else {
            ui_draw_text(LR_CARD_X + 12, LR_CARD_Y + 32, "--",
                         UI_FONT_TITLE, LR_TEXT_DIM);
        }
    }

    /* ---- Proximity warning banner ---- */
    if (dirty_top < LR_WARN_Y + LR_WARN_H && dirty_bot > LR_WARN_Y) {
        ui_rect_t warn = {LR_CARD_X, LR_WARN_Y, LR_CARD_W, LR_WARN_H};
        if (too_close) {
            ui_draw_fill_round_rect(&warn, 6, LR_WARN_COLOR);
            ui_draw_text(LR_CARD_X + 12, LR_WARN_Y + LR_WARN_H / 2 - 6,
                         "TOO CLOSE! Keep 10cm away from screen",
                         UI_FONT_BODY, UI_HEX(0xFFFFFF));
        } else {
            ui_draw_fill_round_rect(&warn, 6, LR_CARD_BG);
            ui_draw_round_rect_border(&warn, 6, LR_CARD_BORDER, 1);
            ui_draw_text(LR_CARD_X + 12, LR_WARN_Y + LR_WARN_H / 2 - 6,
                         "Safe distance (>= 10cm)",
                         UI_FONT_BODY, LR_TEXT_DIM);
        }
    }

    /* ---- Chart ---- */
    if (dirty_top < LR_CHART_Y + LR_CHART_H && dirty_bot > LR_CHART_Y) {
        ui_rect_t chart_rect = {LR_CHART_X, LR_CHART_Y, LR_CHART_W, LR_CHART_H};
        lr_draw_chart(&chart_rect, &s_dist_hist, 0, LR_CHART_Y_MAX, LR_OK_COLOR);

        /* Y axis labels */
        char label[8];
        snprintf(label, sizeof(label), "%d", LR_CHART_Y_MAX);
        ui_draw_text(LR_CHART_X + LR_CHART_PAD_X + 2,
                     LR_CHART_Y + LR_CHART_PAD_Y,
                     label, UI_FONT_BODY, LR_TEXT_DIM);
        snprintf(label, sizeof(label), "%d", 0);
        ui_draw_text(LR_CHART_X + LR_CHART_PAD_X + 2,
                     LR_CHART_Y + LR_CHART_H - LR_CHART_PAD_Y - 12,
                     label, UI_FONT_BODY, LR_TEXT_DIM);
        /* Threshold label */
        ui_draw_text(LR_CHART_X + LR_CHART_W - 70,
                     LR_CHART_Y + LR_CHART_PAD_Y + 12,
                     "10cm limit", UI_FONT_BODY, LR_THRESHOLD_COLOR);
    }

    /* ---- Stats ---- */
    if (dirty_top < LR_STATS_Y + LR_STATS_H && dirty_bot > LR_STATS_Y) {
        ui_rect_t stats_bg = {LR_CARD_X, LR_STATS_Y, LR_CARD_W, LR_STATS_H};
        ui_draw_fill_round_rect(&stats_bg, 4, LR_CARD_BG);
        ui_draw_round_rect_border(&stats_bg, 4, LR_CARD_BORDER, 1);

        if (s_dist_hist.count > 0) {
            char line1[40], line2[24];
            snprintf(line1, sizeof(line1), "Min: %d mm   Max: %d mm",
                     lr_history_min(&s_dist_hist),
                     lr_history_max(&s_dist_hist));
            snprintf(line2, sizeof(line2), "Samples: %d",
                     s_dist_hist.count);
            ui_draw_text(LR_CARD_X + 10, LR_STATS_Y + 6, line1,
                         UI_FONT_BODY, LR_TEXT);
            ui_draw_text(LR_CARD_X + 10, LR_STATS_Y + 26, line2,
                         UI_FONT_BODY, LR_TEXT_DIM);
        } else {
            ui_draw_text(LR_CARD_X + 10, LR_STATS_Y + 20, "No data",
                         UI_FONT_BODY, LR_TEXT_DIM);
        }
    }

    /* ---- Bottom status bar ---- */
    if (dirty_bot > LR_CTRL_Y) {
        ui_rect_t ctrl_bg = {0, LR_CTRL_Y, LR_SCREEN_W, LR_CTRL_H};
        ui_draw_fill_rect(&ctrl_bg, LR_BG);

        if (s_data_valid) {
            ui_draw_fill_circle(16, LR_CTRL_Y + LR_CTRL_H / 2, 5,
                                too_close ? LR_WARN_COLOR : LR_OK_COLOR);
            ui_draw_text(28, LR_CTRL_Y + LR_CTRL_H / 2 - 6,
                         "Receiving data", UI_FONT_BODY, LR_TEXT);
        } else {
            ui_draw_text(12, LR_CTRL_Y + LR_CTRL_H / 2 - 6,
                         "Waiting for data...", UI_FONT_BODY, LR_TEXT_DIM);
        }
    }
}

static bool lr_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;
    (void)e;
    return false;
}

/*=============================================================================
 *  Init
 *=============================================================================*/

void app_lrange_init(void)
{
    ui_app_page_init(&s_app_lrange, "L-Range", 0x10C);

    /* Initialize state */
    s_data_valid = false;
    s_out_of_range = false;
    s_distance_mm = 0;
    s_last_err = 0;
    memset(&s_dist_hist, 0, sizeof(s_dist_hist));

    /* Widgets */
    int widx = 0;

    /* Back button and title */
    s_lrange_widgets[widx++] = (ui_widget_t *)&s_app_lrange.btn_back;
    s_lrange_widgets[widx++] = (ui_widget_t *)&s_app_lrange.lbl_title;

    /* Touch area (for event capture) */
    {
        ui_rect_t r = {0, APP_TITLE_BAR_H, LR_SCREEN_W,
                       LR_SCREEN_H - APP_TITLE_BAR_H};
        ui_widget_init(&s_touch_area, &r);
        s_touch_area.bg_color = UI_COLOR_TRANSPARENT;
    }
    s_lrange_widgets[widx++] = &s_touch_area;

    ui_page_set_widgets(&s_app_lrange.page, s_lrange_widgets, (uint16_t)widx);
    ui_page_set_callbacks(&s_app_lrange.page, lr_page_enter, lr_page_exit,
                          lr_page_draw, NULL);
    ui_page_set_event_cb(&s_app_lrange.page, lr_page_event);
    ui_page_register(&s_app_lrange.page);
}

ui_page_t *app_lrange_get_page(void)
{
    return &s_app_lrange.page;
}
