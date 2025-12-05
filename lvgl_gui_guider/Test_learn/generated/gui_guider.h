/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"


typedef struct
{
  
	lv_obj_t *main;
	bool main_del;
	lv_obj_t *main_menu_1;
	lv_obj_t *main_menu_1_sidebar_page;
	lv_obj_t *main_menu_1_subpage_1_cont;
	lv_obj_t *main_menu_1_cont_1;
	lv_obj_t *main_menu_1_label_1;
	lv_obj_t *main_menu_1_subpage_2_cont;
	lv_obj_t *main_menu_1_cont_2;
	lv_obj_t *main_menu_1_label_2;
	lv_obj_t *main_menu_1_subpage_3_cont;
	lv_obj_t *main_menu_1_cont_3;
	lv_obj_t *main_menu_1_label_3;
	lv_obj_t *main_menu_1_subpage_4_cont;
	lv_obj_t *main_menu_1_cont_4;
	lv_obj_t *main_menu_1_label_4;
	lv_obj_t *main_datetext_1;
	lv_obj_t *main_digital_clock_1;
	lv_obj_t *main_img_1;
	lv_obj_t *main_qrcode_1;
	lv_obj_t *main_btn_music;
	lv_obj_t *main_btn_music_label;
	lv_obj_t *main_btn_dic;
	lv_obj_t *main_btn_dic_label;
	lv_obj_t *main_btn_wifi;
	lv_obj_t *main_btn_wifi_label;
	lv_obj_t *main_btn_charge;
	lv_obj_t *main_btn_charge_label;
	lv_obj_t *main_btn_img;
	lv_obj_t *main_btn_img_label;
	lv_obj_t *main_btn_ble;
	lv_obj_t *main_btn_ble_label;
	lv_obj_t *main_btn_power;
	lv_obj_t *main_btn_power_label;
	lv_obj_t *main_btn_usb;
	lv_obj_t *main_btn_usb_label;
	lv_obj_t *main_btn_edit;
	lv_obj_t *main_btn_edit_label;
	lv_obj_t *main_btn_play;
	lv_obj_t *main_btn_play_label;
	lv_obj_t *main_tabview_1;
	lv_obj_t *main_tabview_1_tab_1;
	lv_obj_t *main_tabview_1_tab_2;
	lv_obj_t *main_tabview_1_tab_3;
	lv_obj_t *main_sw_1;
	lv_obj_t *main_sw_2;
	lv_obj_t *main_slider_1;
	lv_obj_t *main_cb_1;
	lv_obj_t *main_ddlist_1;
	lv_obj_t *main_label_1;
	lv_obj_t *main_label_2;
	lv_obj_t *main_label_3;
	lv_obj_t *main_label_4;
	lv_obj_t *main_label_5;
	lv_obj_t *game2048;
	bool game2048_del;
	lv_obj_t *game2048_label_score_title;
	lv_obj_t *game2048_label_best_title;
	lv_obj_t *game2048_label_score;
	lv_obj_t *game2048_label_best;
	lv_obj_t *game2048_label_2048;
	lv_obj_t *game2048_btnm_2048;
	lv_obj_t *game2048_btn_up;
	lv_obj_t *game2048_btn_up_label;
	lv_obj_t *game2048_btn_new_game;
	lv_obj_t *game2048_btn_new_game_label;
	lv_obj_t *game2048_btn_left;
	lv_obj_t *game2048_btn_left_label;
	lv_obj_t *game2048_btn_down;
	lv_obj_t *game2048_btn_down_label;
	lv_obj_t *game2048_btn_right;
	lv_obj_t *game2048_btn_right_label;
	lv_obj_t *game2048_btn_back;
	lv_obj_t *game2048_btn_back_label;
	lv_obj_t *game2048_cont_msgbox;
	lv_obj_t *game2048_btn_again;
	lv_obj_t *game2048_btn_again_label;
	lv_obj_t *game2048_label_results_title;
	lv_obj_t *game2048_label_results;
	lv_obj_t *game2048_label_warning;
}lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui * ui);

void ui_init_style(lv_style_t * style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t ** new_scr, bool new_scr_del, bool * old_scr_del, ui_setup_scr_t setup_scr,
                           lv_screen_load_anim_t anim_type, uint32_t time, uint32_t delay, bool is_clean, bool auto_del);

void ui_animation(void * var, uint32_t duration, int32_t delay, int32_t start_value, int32_t end_value, lv_anim_path_cb_t path_cb,
                  uint32_t repeat_cnt, uint32_t repeat_delay, uint32_t playback_time, uint32_t playback_delay,
                  lv_anim_exec_xcb_t exec_cb, lv_anim_start_cb_t start_cb, lv_anim_completed_cb_t ready_cb, lv_anim_deleted_cb_t deleted_cb);


void init_scr_del_flag(lv_ui *ui);

void setup_bottom_layer(void);

void setup_ui(lv_ui *ui);

void video_play(lv_ui *ui);

void init_keyboard(lv_ui *ui);

extern lv_ui guider_ui;


void setup_scr_main(lv_ui *ui);
void setup_scr_game2048(lv_ui *ui);
LV_IMAGE_DECLARE(_logo_RGB565A8_120x30_RLE);

LV_FONT_DECLARE(lv_font_Alatsi_Regular_24)
LV_FONT_DECLARE(lv_font_montserratMedium_26)
LV_FONT_DECLARE(lv_font_Alatsi_Regular_33)
LV_FONT_DECLARE(lv_font_Alatsi_Regular_20)
LV_FONT_DECLARE(lv_font_montserratMedium_32)
LV_FONT_DECLARE(lv_font_montserratMedium_12)
LV_FONT_DECLARE(lv_font_montserratMedium_16)
LV_FONT_DECLARE(lv_font_Alatsi_Regular_36)
LV_FONT_DECLARE(lv_font_Alatsi_Regular_100)
LV_FONT_DECLARE(lv_font_montserratMedium_58)
LV_FONT_DECLARE(lv_font_montserratMedium_33)
LV_FONT_DECLARE(lv_font_Alatsi_Regular_43)
LV_FONT_DECLARE(lv_font_Alatsi_Regular_66)


#ifdef __cplusplus
}
#endif
#endif
