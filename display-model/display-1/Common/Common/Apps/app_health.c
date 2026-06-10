/********************************** (C) COPYRIGHT *******************************
* File Name          : app_health.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2026/06/10
* Description        : Health monitor app (V2.0).
*                      Real-time HR/SpO2/HRV display with line charts.
*                      Data received via DISP_EXT_SUBMODEL_EVENT from Core.
*                      History stored only while app is active (saves RAM).
********************************************************************************/
#include "app_health.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 *  Layout Configuration (800x480)
 *=============================================================================*/

#define HLTH_SCREEN_W           UI_SCREEN_WIDTH
#define HLTH_SCREEN_H           UI_SCREEN_HEIGHT
#define HLTH_CONTENT_Y          (APP_TITLE_BAR_H + 4)
#define HLTH_CONTENT_H          (HLTH_SCREEN_H - APP_TITLE_BAR_H - 4)

/* Three columns for HR / SpO2 / HRV */
#define HLTH_COL_GAP            8
#define HLTH_COL_W              ((HLTH_SCREEN_W - 2 * HLTH_COL_GAP) / 3)
#define HLTH_COL0_X             HLTH_COL_GAP
#define HLTH_COL1_X             (HLTH_COL0_X + HLTH_COL_W + HLTH_COL_GAP)
#define HLTH_COL2_X             (HLTH_COL1_X + HLTH_COL_W + HLTH_COL_GAP)

/* Value card */
#define HLTH_CARD_Y             HLTH_CONTENT_Y
#define HLTH_CARD_H             80
#define HLTH_CARD_R             8

/* Chart area */
#define HLTH_CHART_Y            (HLTH_CARD_Y + HLTH_CARD_H + 8)
#define HLTH_CHART_H            140
#define HLTH_CHART_PAD_X        4
#define HLTH_CHART_PAD_Y        4

/* Stats area */
#define HLTH_STATS_Y            (HLTH_CHART_Y + HLTH_CHART_H + 8)
#define HLTH_STATS_H            60

/* Bottom status bar */
#define HLTH_CTRL_Y             (HLTH_SCREEN_H - 44)
#define HLTH_CTRL_H             40

/*=============================================================================
 *  History Buffer
 *=============================================================================*/

#define HLTH_HISTORY_LEN        60  /* ~5 min at 5s interval */

typedef struct {
    uint8_t  data[HLTH_HISTORY_LEN];
    uint16_t count;     /* Total samples received (wraps) */
    uint16_t write_idx; /* Circular buffer write position */
    uint32_t sum;       /* Running sum for average (last 255 samples) */
    uint8_t  sum_count; /* Number of samples in sum */
} hlth_history_t;

static void hlth_history_push(hlth_history_t *h, uint8_t val)
{
    h->data[h->write_idx] = val;
    h->write_idx = (h->write_idx + 1) % HLTH_HISTORY_LEN;
    if (h->count < HLTH_HISTORY_LEN)
        h->count++;
    /* Running sum for average */
    if (h->sum_count < 255) {
        h->sum += val;
        h->sum_count++;
    } else {
        /* Recalculate from buffer */
        uint32_t s = 0;
        uint16_t n = (h->count < HLTH_HISTORY_LEN) ? h->count : HLTH_HISTORY_LEN;
        for (uint16_t i = 0; i < n; i++)
            s += h->data[i];
        h->sum = s;
        h->sum_count = (uint8_t)n;
    }
}

static uint8_t hlth_history_avg(const hlth_history_t *h)
{
    if (h->sum_count == 0) return 0;
    return (uint8_t)(h->sum / h->sum_count);
}

static uint8_t hlth_history_min(const hlth_history_t *h)
{
    if (h->count == 0) return 0;
    uint16_t n = (h->count < HLTH_HISTORY_LEN) ? h->count : HLTH_HISTORY_LEN;
    uint8_t mn = 255;
    for (uint16_t i = 0; i < n; i++) {
        if (h->data[i] < mn) mn = h->data[i];
    }
    return mn;
}

static uint8_t hlth_history_max(const hlth_history_t *h)
{
    if (h->count == 0) return 0;
    uint16_t n = (h->count < HLTH_HISTORY_LEN) ? h->count : HLTH_HISTORY_LEN;
    uint8_t mx = 0;
    for (uint16_t i = 0; i < n; i++) {
        if (h->data[i] > mx) mx = h->data[i];
    }
    return mx;
}

/*=============================================================================
 *  Colors
 *=============================================================================*/

#define HLTH_BG                 UI_COLOR_BG_MAIN
#define HLTH_CARD_BG            UI_COLOR_BG_CARD
#define HLTH_CARD_BORDER        UI_HEX(0xE0E0E0)
#define HLTH_TEXT               UI_COLOR_TEXT_PRIMARY
#define HLTH_TEXT_DIM           UI_COLOR_TEXT_SECONDARY
#define HLTH_CHART_BG           UI_HEX(0xFAFAFA)
#define HLTH_CHART_GRID         UI_HEX(0xE8E8E8)
#define HLTH_CHART_BORDER       UI_HEX(0xD0D0D0)

/* Per-metric accent colors */
#define HLTH_HR_COLOR           UI_HEX(0xE53935)   /* Red */
#define HLTH_SPO2_COLOR         UI_HEX(0x1E88E5)   /* Blue */
#define HLTH_HRV_COLOR          UI_HEX(0x43A047)   /* Green */

#define HLTH_HR_BG              UI_HEX(0xFFEBEE)
#define HLTH_SPO2_BG            UI_HEX(0xE3F2FD)
#define HLTH_HRV_BG             UI_HEX(0xE8F5E9)

/*=============================================================================
 *  State
 *=============================================================================*/

static ui_app_page_t s_app_health;

/* Current values */
static uint8_t  s_hr;          /* Heart rate BPM */
static uint8_t  s_spo2;        /* SpO2 % */
static uint16_t s_hrv;         /* HRV ms */
static bool     s_data_valid;  /* Have received at least one data report */

/* History (allocated only in this file, valid while app is active) */
static hlth_history_t s_hr_hist;
static hlth_history_t s_spo2_hist;
static hlth_history_t s_hrv_hist;

/* Widgets */
static ui_widget_t s_touch_area;
static ui_widget_t *s_health_widgets[3]; /* back + title + touch */

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void hlth_on_submodel_event(uint8_t sub_type, uint8_t sub_cmd,
                                    const uint8_t *evt_data, uint8_t evt_len);
static void hlth_invalidate_col(int col);

static uart_submodel_cb_t s_health_submodel_cb = {
    .on_submodel_event = hlth_on_submodel_event,
};

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void hlth_invalidate_col(int col)
{
    int16_t x;
    switch (col) {
    case 0: x = HLTH_COL0_X; break;
    case 1: x = HLTH_COL1_X; break;
    case 2: x = HLTH_COL2_X; break;
    default: return;
    }
    ui_rect_t r = {x, HLTH_CONTENT_Y, HLTH_COL_W, HLTH_CONTENT_H - HLTH_CTRL_H};
    ui_page_invalidate(&r);
}

/*=============================================================================
 *  Submodel Event Handler
 *=============================================================================*/

static void hlth_on_submodel_event(uint8_t sub_type, uint8_t sub_cmd,
                                    const uint8_t *evt_data, uint8_t evt_len)
{
    if (sub_type != SUBMODEL_HEALTH)
        return;

    switch (sub_cmd) {
    case HEALTH_EVT_DATA_REPORT:
    {
        if (!evt_data || evt_len < 4)
            return;

        uint8_t hr = evt_data[0];
        uint8_t spo2 = evt_data[1];
        uint16_t hrv = ((uint16_t)evt_data[2] << 8) | evt_data[3];

        s_hr = hr;
        s_spo2 = spo2;
        s_hrv = hrv;
        s_data_valid = true;

        /* Push to history (HRV clamped to 255 for chart display) */
        hlth_history_push(&s_hr_hist, hr);
        hlth_history_push(&s_spo2_hist, spo2);
        hlth_history_push(&s_hrv_hist, (hrv > 255) ? 255 : (uint8_t)hrv);

        /* Invalidate all columns */
        hlth_invalidate_col(0);
        hlth_invalidate_col(1);
        hlth_invalidate_col(2);
        break;
    }
    default:
        break;
    }
}

/*=============================================================================
 *  Chart Drawing
 *=============================================================================*/

/* Draw a line chart for a history buffer within the given rect.
 * y_min/y_max define the value range for the Y axis. */
static void hlth_draw_chart(const ui_rect_t *rect, const hlth_history_t *hist,
                             uint8_t y_min, uint8_t y_max, ui_color_t line_color)
{
    int16_t x = rect->x + HLTH_CHART_PAD_X;
    int16_t y = rect->y + HLTH_CHART_PAD_Y;
    int16_t w = rect->w - 2 * HLTH_CHART_PAD_X;
    int16_t h = rect->h - 2 * HLTH_CHART_PAD_Y;

    /* Background */
    ui_draw_fill_rect(rect, HLTH_CHART_BG);
    ui_draw_rect_border(rect, HLTH_CHART_BORDER, 1);

    /* Grid lines (3 horizontal) */
    for (int i = 1; i <= 3; i++) {
        int16_t gy = y + h * i / 4;
        ui_draw_hline(x, gy, w, HLTH_CHART_GRID);
    }

    uint16_t n = (hist->count < HLTH_HISTORY_LEN) ? hist->count : HLTH_HISTORY_LEN;
    if (n < 2) {
        /* No data yet */
        ui_draw_text(rect->x + rect->w / 2 - 30, rect->y + rect->h / 2 - 6,
                     "No data", &font_montserrat_12, HLTH_TEXT_DIM);
        return;
    }

    /* Value range */
    uint16_t range = y_max - y_min;
    if (range == 0) range = 1;

    /* Draw line segments */
    int16_t prev_px = 0, prev_py = 0;
    for (uint16_t i = 0; i < n; i++) {
        /* Read from circular buffer: oldest first */
        uint16_t idx;
        if (hist->count >= HLTH_HISTORY_LEN)
            idx = (hist->write_idx + i) % HLTH_HISTORY_LEN;
        else
            idx = i;

        uint8_t val = hist->data[idx];
        int16_t px = x + (int16_t)((uint32_t)w * i / (n - 1));
        int16_t py = y + h - (int16_t)((uint32_t)(val - y_min) * h / range);
        /* Clamp */
        if (py < y) py = y;
        if (py > y + h) py = y + h;

        if (i > 0) {
            ui_draw_line(prev_px, prev_py, px, py, line_color);
        }
        prev_px = px;
        prev_py = py;
    }

    /* Fill area under curve with a subtle tinted color */
    ui_color_t fill_colors[3] = {
        UI_HEX(0xF8D7DA),  /* Light red for HR */
        UI_HEX(0xD0E8FC),  /* Light blue for SpO2 */
        UI_HEX(0xD4EDDA),  /* Light green for HRV */
    };
    ui_color_t fill_color = fill_colors[0];
    /* Pick fill color based on line_color matching */
    if (line_color == HLTH_SPO2_COLOR) fill_color = fill_colors[1];
    else if (line_color == HLTH_HRV_COLOR) fill_color = fill_colors[2];

    for (uint16_t i = 0; i < n; i++) {
        uint16_t idx;
        if (hist->count >= HLTH_HISTORY_LEN)
            idx = (hist->write_idx + i) % HLTH_HISTORY_LEN;
        else
            idx = i;

        uint8_t val = hist->data[idx];
        int16_t px = x + (int16_t)((uint32_t)w * i / (n - 1));
        int16_t py = y + h - (int16_t)((uint32_t)(val - y_min) * h / range);
        if (py < y) py = y;
        if (py > y + h) py = y + h;

        if (py < y + h - 1) {
            ui_draw_vline(px, py, y + h - py, fill_color);
        }
    }
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void hlth_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetSubmodelCallbacks(&s_health_submodel_cb);

    /* Reset state */
    s_data_valid = false;
    s_hr = 0;
    s_spo2 = 0;
    s_hrv = 0;
    memset(&s_hr_hist, 0, sizeof(s_hr_hist));
    memset(&s_spo2_hist, 0, sizeof(s_spo2_hist));
    memset(&s_hrv_hist, 0, sizeof(s_hrv_hist));
}

static void hlth_page_exit(ui_page_t *page)
{
    (void)page;
    UART_ClearSubmodelCallbacks();
}

static void hlth_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;

    /* Title bar background */
    ui_rect_t bar = {0, 0, HLTH_SCREEN_W, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    int16_t dirty_top = dirty->y;
    int16_t dirty_bot = dirty->y + dirty->h;

    /* Column data */
    struct {
        int16_t x;
        const char *label;
        const char *unit;
        ui_color_t accent;
        ui_color_t card_bg;
        uint8_t cur_val;
        const hlth_history_t *hist;
        uint8_t y_min;
        uint8_t y_max;
    } cols[3] = {
        { HLTH_COL0_X, "Heart Rate", "BPM", HLTH_HR_COLOR, HLTH_HR_BG,
          s_hr, &s_hr_hist, 40, 200 },
        { HLTH_COL1_X, "SpO2", "%", HLTH_SPO2_COLOR, HLTH_SPO2_BG,
          s_spo2, &s_spo2_hist, 80, 100 },
        { HLTH_COL2_X, "HRV", "ms", HLTH_HRV_COLOR, HLTH_HRV_BG,
          (s_hrv > 255) ? 255 : (uint8_t)s_hrv, &s_hrv_hist, 0, 200 },
    };

    for (int c = 0; c < 3; c++) {
        int16_t cx = cols[c].x;

        /* Skip if not in dirty region */
        if (dirty_bot <= HLTH_CONTENT_Y || dirty_top >= HLTH_CTRL_Y)
            if (dirty->x + dirty->w <= cx || dirty->x >= cx + HLTH_COL_W)
                continue;

        /* Value card */
        if (dirty_top < HLTH_CARD_Y + HLTH_CARD_H && dirty_bot > HLTH_CARD_Y) {
            ui_rect_t card = {cx, HLTH_CARD_Y, HLTH_COL_W, HLTH_CARD_H};
            ui_draw_fill_round_rect(&card, HLTH_CARD_R, cols[c].card_bg);
            ui_draw_round_rect_border(&card, HLTH_CARD_R, cols[c].accent, 2);

            /* Label */
            ui_draw_text(cx + 10, HLTH_CARD_Y + 6, cols[c].label,
                         &font_montserrat_12, cols[c].accent);

            /* Current value */
            if (s_data_valid) {
                char val_str[8];
                if (c == 2 && s_hrv > 255) {
                    snprintf(val_str, sizeof(val_str), "%d", s_hrv);
                } else {
                    snprintf(val_str, sizeof(val_str), "%d", cols[c].cur_val);
                }
                ui_draw_text(cx + 10, HLTH_CARD_Y + 24, val_str,
                             &font_montserrat_16, HLTH_TEXT);

                /* Unit */
                ui_draw_text(cx + 10 + ui_text_width(val_str, &font_montserrat_16) + 4,
                             HLTH_CARD_Y + 30, cols[c].unit,
                             &font_montserrat_12, HLTH_TEXT_DIM);
            } else {
                ui_draw_text(cx + 10, HLTH_CARD_Y + 28, "--",
                             &font_montserrat_16, HLTH_TEXT_DIM);
            }

            /* Average */
            if (cols[c].hist->count > 0) {
                char avg_str[24];
                snprintf(avg_str, sizeof(avg_str), "Avg: %d",
                         hlth_history_avg(cols[c].hist));
                ui_draw_text(cx + 10, HLTH_CARD_Y + 54, avg_str,
                             &font_montserrat_12, HLTH_TEXT_DIM);
            }
        }

        /* Chart */
        if (dirty_top < HLTH_CHART_Y + HLTH_CHART_H && dirty_bot > HLTH_CHART_Y) {
            ui_rect_t chart_rect = {cx, HLTH_CHART_Y, HLTH_COL_W, HLTH_CHART_H};
            hlth_draw_chart(&chart_rect, cols[c].hist,
                            cols[c].y_min, cols[c].y_max, cols[c].accent);

            /* Y axis labels */
            char label[8];
            snprintf(label, sizeof(label), "%d", cols[c].y_max);
            ui_draw_text(cx + HLTH_CHART_PAD_X + 2, HLTH_CHART_Y + HLTH_CHART_PAD_Y,
                         label, &font_montserrat_12, HLTH_TEXT_DIM);
            snprintf(label, sizeof(label), "%d", cols[c].y_min);
            ui_draw_text(cx + HLTH_CHART_PAD_X + 2,
                         HLTH_CHART_Y + HLTH_CHART_H - HLTH_CHART_PAD_Y - 12,
                         label, &font_montserrat_12, HLTH_TEXT_DIM);
        }

        /* Stats */
        if (dirty_top < HLTH_STATS_Y + HLTH_STATS_H && dirty_bot > HLTH_STATS_Y) {
            ui_rect_t stats_bg = {cx, HLTH_STATS_Y, HLTH_COL_W, HLTH_STATS_H};
            ui_draw_fill_round_rect(&stats_bg, 4, HLTH_CARD_BG);
            ui_draw_round_rect_border(&stats_bg, 4, HLTH_CARD_BORDER, 1);

            if (cols[c].hist->count > 0) {
                char line1[24], line2[24];
                snprintf(line1, sizeof(line1), "Min: %d  Max: %d",
                         hlth_history_min(cols[c].hist),
                         hlth_history_max(cols[c].hist));
                snprintf(line2, sizeof(line2), "Samples: %d",
                         cols[c].hist->count);
                ui_draw_text(cx + 8, HLTH_STATS_Y + 6, line1,
                             &font_montserrat_12, HLTH_TEXT);
                ui_draw_text(cx + 8, HLTH_STATS_Y + 26, line2,
                             &font_montserrat_12, HLTH_TEXT_DIM);
            } else {
                ui_draw_text(cx + 8, HLTH_STATS_Y + 20, "No data",
                             &font_montserrat_12, HLTH_TEXT_DIM);
            }
        }
    }

    /* Bottom status bar */
    if (dirty_bot > HLTH_CTRL_Y) {
        ui_rect_t ctrl_bg = {0, HLTH_CTRL_Y, HLTH_SCREEN_W, HLTH_CTRL_H};
        ui_draw_fill_rect(&ctrl_bg, HLTH_BG);

        /* Status indicator */
        if (s_data_valid) {
            ui_draw_fill_circle(16, HLTH_CTRL_Y + HLTH_CTRL_H / 2, 5,
                                HLTH_HR_COLOR);
            ui_draw_text(28, HLTH_CTRL_Y + HLTH_CTRL_H / 2 - 6,
                         "Receiving data", &font_montserrat_12, HLTH_TEXT);
        } else {
            ui_draw_text(12, HLTH_CTRL_Y + HLTH_CTRL_H / 2 - 6,
                         "Waiting for data...", &font_montserrat_12, HLTH_TEXT_DIM);
        }
    }
}

static bool hlth_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;
    (void)e;
    return false;
}

/*=============================================================================
 *  Init
 *=============================================================================*/

void app_health_init(void)
{
    ui_app_page_init(&s_app_health, "Health", 0x109);

    /* Initialize state */
    s_data_valid = false;
    s_hr = 0;
    s_spo2 = 0;
    s_hrv = 0;
    memset(&s_hr_hist, 0, sizeof(s_hr_hist));
    memset(&s_spo2_hist, 0, sizeof(s_spo2_hist));
    memset(&s_hrv_hist, 0, sizeof(s_hrv_hist));

    /* Widgets */
    int widx = 0;

    /* Back button and title */
    s_health_widgets[widx++] = (ui_widget_t *)&s_app_health.btn_back;
    s_health_widgets[widx++] = (ui_widget_t *)&s_app_health.lbl_title;

    /* Touch area (for event capture) */
    {
        ui_rect_t r = {0, APP_TITLE_BAR_H, HLTH_SCREEN_W,
                       HLTH_SCREEN_H - APP_TITLE_BAR_H};
        ui_widget_init(&s_touch_area, &r);
        s_touch_area.bg_color = UI_COLOR_TRANSPARENT;
    }
    s_health_widgets[widx++] = &s_touch_area;

    ui_page_set_widgets(&s_app_health.page, s_health_widgets, (uint16_t)widx);
    ui_page_set_callbacks(&s_app_health.page, hlth_page_enter, hlth_page_exit,
                          hlth_page_draw, NULL);
    ui_page_set_event_cb(&s_app_health.page, hlth_page_event);
    ui_page_register(&s_app_health.page);
}

ui_page_t *app_health_get_page(void)
{
    return &s_app_health.page;
}
