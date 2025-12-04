/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"



int main_digital_clock_1_min_value = 25;
int main_digital_clock_1_hour_value = 11;
int main_digital_clock_1_sec_value = 50;
char main_digital_clock_1_meridiem[] = "AM";
void setup_scr_main(lv_ui *ui)
{
    //Write codes main
    ui->main = lv_obj_create(NULL);
    lv_obj_set_size(ui->main, 800, 480);
    lv_obj_set_scrollbar_mode(ui->main, LV_SCROLLBAR_MODE_OFF);

    //Write style for main, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->main, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes main_menu_1
    ui->main_menu_1 = lv_menu_create(ui->main);
    lv_obj_set_pos(ui->main_menu_1, 0, 0);
    lv_obj_set_size(ui->main_menu_1, 800, 480);
    lv_obj_set_scrollbar_mode(ui->main_menu_1, LV_SCROLLBAR_MODE_OFF);

    //Create sidebar page for menu main_menu_1
    ui->main_menu_1_sidebar_page = lv_menu_page_create(ui->main_menu_1, "Menu");
    lv_menu_set_sidebar_page(ui->main_menu_1, ui->main_menu_1_sidebar_page);
    lv_obj_set_scrollbar_mode(ui->main_menu_1_sidebar_page, LV_SCROLLBAR_MODE_OFF);

    //Create subpage for main_menu_1
    lv_obj_t * main_menu_1_subpage_1 = lv_menu_page_create(ui->main_menu_1, NULL);
    ui->main_menu_1_subpage_1_cont = lv_menu_cont_create(main_menu_1_subpage_1);
    lv_obj_set_layout(ui->main_menu_1_subpage_1_cont, LV_LAYOUT_NONE);
    ui->main_menu_1_cont_1 = lv_menu_cont_create(ui->main_menu_1_sidebar_page);
    ui->main_menu_1_label_1 = lv_label_create(ui->main_menu_1_cont_1);
    lv_label_set_text(ui->main_menu_1_label_1, "Main");
    lv_obj_set_size(ui->main_menu_1_label_1, LV_PCT(100), LV_SIZE_CONTENT);
    lv_label_set_long_mode(ui->main_menu_1_label_1, LV_LABEL_LONG_CLIP);
    lv_obj_set_scrollbar_mode(main_menu_1_subpage_1, LV_SCROLLBAR_MODE_OFF);
    lv_menu_set_load_page_event(ui->main_menu_1, ui->main_menu_1_cont_1, main_menu_1_subpage_1);

    //Create subpage for main_menu_1
    lv_obj_t * main_menu_1_subpage_2 = lv_menu_page_create(ui->main_menu_1, NULL);
    ui->main_menu_1_subpage_2_cont = lv_menu_cont_create(main_menu_1_subpage_2);
    lv_obj_set_layout(ui->main_menu_1_subpage_2_cont, LV_LAYOUT_NONE);
    ui->main_menu_1_cont_2 = lv_menu_cont_create(ui->main_menu_1_sidebar_page);
    ui->main_menu_1_label_2 = lv_label_create(ui->main_menu_1_cont_2);
    lv_label_set_text(ui->main_menu_1_label_2, "Software");
    lv_obj_set_size(ui->main_menu_1_label_2, LV_PCT(100), LV_SIZE_CONTENT);
    lv_label_set_long_mode(ui->main_menu_1_label_2, LV_LABEL_LONG_CLIP);
    lv_obj_set_scrollbar_mode(main_menu_1_subpage_2, LV_SCROLLBAR_MODE_OFF);
    lv_menu_set_load_page_event(ui->main_menu_1, ui->main_menu_1_cont_2, main_menu_1_subpage_2);

    //Create subpage for main_menu_1
    lv_obj_t * main_menu_1_subpage_3 = lv_menu_page_create(ui->main_menu_1, NULL);
    ui->main_menu_1_subpage_3_cont = lv_menu_cont_create(main_menu_1_subpage_3);
    lv_obj_set_layout(ui->main_menu_1_subpage_3_cont, LV_LAYOUT_NONE);
    ui->main_menu_1_cont_3 = lv_menu_cont_create(ui->main_menu_1_sidebar_page);
    ui->main_menu_1_label_3 = lv_label_create(ui->main_menu_1_cont_3);
    lv_label_set_text(ui->main_menu_1_label_3, "Models");
    lv_obj_set_size(ui->main_menu_1_label_3, LV_PCT(100), LV_SIZE_CONTENT);
    lv_label_set_long_mode(ui->main_menu_1_label_3, LV_LABEL_LONG_CLIP);
    lv_obj_set_scrollbar_mode(main_menu_1_subpage_3, LV_SCROLLBAR_MODE_OFF);
    lv_menu_set_load_page_event(ui->main_menu_1, ui->main_menu_1_cont_3, main_menu_1_subpage_3);

    //Create subpage for main_menu_1
    lv_obj_t * main_menu_1_subpage_4 = lv_menu_page_create(ui->main_menu_1, NULL);
    ui->main_menu_1_subpage_4_cont = lv_menu_cont_create(main_menu_1_subpage_4);
    lv_obj_set_layout(ui->main_menu_1_subpage_4_cont, LV_LAYOUT_NONE);
    ui->main_menu_1_cont_4 = lv_menu_cont_create(ui->main_menu_1_sidebar_page);
    ui->main_menu_1_label_4 = lv_label_create(ui->main_menu_1_cont_4);
    lv_label_set_text(ui->main_menu_1_label_4, "Options");
    lv_obj_set_size(ui->main_menu_1_label_4, LV_PCT(100), LV_SIZE_CONTENT);
    lv_label_set_long_mode(ui->main_menu_1_label_4, LV_LABEL_LONG_CLIP);
    lv_obj_set_scrollbar_mode(main_menu_1_subpage_4, LV_SCROLLBAR_MODE_OFF);
    lv_menu_set_load_page_event(ui->main_menu_1, ui->main_menu_1_cont_4, main_menu_1_subpage_4);

    //Write style for main_menu_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->main_menu_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_menu_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_menu_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_menu_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_menu_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_menu_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_margin_hor(ui->main_menu_1_sidebar_page, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_margin_ver(ui->main_menu_1_sidebar_page, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->main_menu_1_sidebar_page, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_menu_1_sidebar_page, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->main_menu_1_sidebar_page, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_menu_1_sidebar_page, lv_color_hex(0xf6f6f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_menu_1_sidebar_page, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_main_menu_1_extra_option_btns_main_default
    static lv_style_t style_main_menu_1_extra_option_btns_main_default;
    ui_init_style(&style_main_menu_1_extra_option_btns_main_default);

    lv_style_set_text_color(&style_main_menu_1_extra_option_btns_main_default, lv_color_hex(0x151212));
    lv_style_set_text_font(&style_main_menu_1_extra_option_btns_main_default, &lv_font_Alatsi_Regular_12);
    lv_style_set_text_opa(&style_main_menu_1_extra_option_btns_main_default, 255);
    lv_style_set_text_align(&style_main_menu_1_extra_option_btns_main_default, LV_TEXT_ALIGN_CENTER);
    lv_style_set_pad_top(&style_main_menu_1_extra_option_btns_main_default, 10);
    lv_style_set_pad_bottom(&style_main_menu_1_extra_option_btns_main_default, 10);
    lv_obj_add_style(ui->main_menu_1_cont_4, &style_main_menu_1_extra_option_btns_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_add_style(ui->main_menu_1_cont_3, &style_main_menu_1_extra_option_btns_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_add_style(ui->main_menu_1_cont_2, &style_main_menu_1_extra_option_btns_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_add_style(ui->main_menu_1_cont_1, &style_main_menu_1_extra_option_btns_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_CHECKED for &style_main_menu_1_extra_option_btns_main_checked
    static lv_style_t style_main_menu_1_extra_option_btns_main_checked;
    ui_init_style(&style_main_menu_1_extra_option_btns_main_checked);

    lv_style_set_text_color(&style_main_menu_1_extra_option_btns_main_checked, lv_color_hex(0x9ab700));
    lv_style_set_text_font(&style_main_menu_1_extra_option_btns_main_checked, &lv_font_montserratMedium_12);
    lv_style_set_text_opa(&style_main_menu_1_extra_option_btns_main_checked, 255);
    lv_style_set_text_align(&style_main_menu_1_extra_option_btns_main_checked, LV_TEXT_ALIGN_CENTER);
    lv_style_set_border_width(&style_main_menu_1_extra_option_btns_main_checked, 0);
    lv_style_set_radius(&style_main_menu_1_extra_option_btns_main_checked, 5);
    lv_style_set_bg_opa(&style_main_menu_1_extra_option_btns_main_checked, 60);
    lv_style_set_bg_color(&style_main_menu_1_extra_option_btns_main_checked, lv_color_hex(0x19a5ff));
    lv_style_set_bg_grad_dir(&style_main_menu_1_extra_option_btns_main_checked, LV_GRAD_DIR_NONE);
    lv_obj_add_style(ui->main_menu_1_cont_4, &style_main_menu_1_extra_option_btns_main_checked, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_add_style(ui->main_menu_1_cont_3, &style_main_menu_1_extra_option_btns_main_checked, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_add_style(ui->main_menu_1_cont_2, &style_main_menu_1_extra_option_btns_main_checked, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_add_style(ui->main_menu_1_cont_1, &style_main_menu_1_extra_option_btns_main_checked, LV_PART_MAIN|LV_STATE_CHECKED);

    //Write style state: LV_STATE_DEFAULT for &style_main_menu_1_extra_main_title_main_default
    static lv_style_t style_main_menu_1_extra_main_title_main_default;
    ui_init_style(&style_main_menu_1_extra_main_title_main_default);

    lv_style_set_text_color(&style_main_menu_1_extra_main_title_main_default, lv_color_hex(0x41485a));
    lv_style_set_text_font(&style_main_menu_1_extra_main_title_main_default, &lv_font_Alatsi_Regular_14);
    lv_style_set_text_opa(&style_main_menu_1_extra_main_title_main_default, 255);
    lv_style_set_text_align(&style_main_menu_1_extra_main_title_main_default, LV_TEXT_ALIGN_CENTER);
    lv_style_set_bg_opa(&style_main_menu_1_extra_main_title_main_default, 0);
    lv_style_set_pad_hor(&style_main_menu_1_extra_main_title_main_default, 5);
    lv_style_set_pad_ver(&style_main_menu_1_extra_main_title_main_default, 5);
    lv_menu_t * main_menu_1_menu= (lv_menu_t *)ui->main_menu_1;
    lv_obj_t * main_menu_1_title = main_menu_1_menu->sidebar_header_title;
    lv_obj_set_size(main_menu_1_title, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_style(lv_menu_get_sidebar_header(ui->main_menu_1), &style_main_menu_1_extra_main_title_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);



    //Write codes main_datetext_1
    ui->main_datetext_1 = lv_label_create(ui->main_menu_1_subpage_1_cont);
    lv_obj_set_pos(ui->main_datetext_1, 190, 10);
    lv_obj_set_size(ui->main_datetext_1, 180, 40);
    lv_label_set_text(ui->main_datetext_1, "2024/04/22");
    lv_obj_set_style_text_align(ui->main_datetext_1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(ui->main_datetext_1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui->main_datetext_1, main_datetext_1_event_handler, LV_EVENT_ALL, NULL);

    //Write style for main_datetext_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->main_datetext_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_datetext_1, &lv_font_Alatsi_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_datetext_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->main_datetext_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_datetext_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->main_datetext_1, 129, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_datetext_1, lv_color_hex(0x52e4f9), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_datetext_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->main_datetext_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_datetext_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->main_datetext_1, 7, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->main_datetext_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->main_datetext_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_datetext_1, 7, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->main_datetext_1, lv_color_hex(0x636d75), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->main_datetext_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->main_datetext_1, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(ui->main_datetext_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(ui->main_datetext_1, 3, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes main_digital_clock_1
    static bool main_digital_clock_1_timer_enabled = false;
    ui->main_digital_clock_1 = lv_label_create(ui->main_menu_1_subpage_1_cont);
    lv_obj_set_pos(ui->main_digital_clock_1, 190, 70);
    lv_obj_set_size(ui->main_digital_clock_1, 180, 36);
    lv_label_set_text(ui->main_digital_clock_1, "11:25:50 AM");
    if (!main_digital_clock_1_timer_enabled) {
        lv_timer_create(main_digital_clock_1_timer, 1000, NULL);
        main_digital_clock_1_timer_enabled = true;
    }

    //Write style for main_digital_clock_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_radius(ui->main_digital_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->main_digital_clock_1, lv_color_hex(0x191717), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_digital_clock_1, &lv_font_Alatsi_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_digital_clock_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->main_digital_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_digital_clock_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->main_digital_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->main_digital_clock_1, 7, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->main_digital_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->main_digital_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->main_digital_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_digital_clock_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->main_digital_clock_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->main_digital_clock_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->main_digital_clock_1, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(ui->main_digital_clock_1, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(ui->main_digital_clock_1, 1, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes main_img_1
    ui->main_img_1 = lv_image_create(ui->main_menu_1_subpage_1_cont);
    lv_obj_set_pos(ui->main_img_1, 14, 435);
    lv_obj_set_size(ui->main_img_1, 120, 30);
    lv_obj_add_flag(ui->main_img_1, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->main_img_1, &_logo_RGB888_120x30_RLE);
    lv_image_set_pivot(ui->main_img_1, 50,50);
    lv_image_set_rotation(ui->main_img_1, 0);

    //Write style for main_img_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->main_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->main_img_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes main_qrcode_1
    ui->main_qrcode_1 = lv_qrcode_create(ui->main_menu_1_subpage_1_cont);
    lv_obj_set_pos(ui->main_qrcode_1, 429, 11);
    lv_obj_set_size(ui->main_qrcode_1, 100, 100);
    lv_qrcode_set_size(ui->main_qrcode_1, 100);
    lv_qrcode_set_dark_color(ui->main_qrcode_1, lv_color_hex(0x000000));
    lv_qrcode_set_light_color(ui->main_qrcode_1, lv_color_hex(0xffffff));
    const char * main_qrcode_1_data = "https://www.dlut.edu.cn/";
    lv_qrcode_update(ui->main_qrcode_1, main_qrcode_1_data, 24);



    //Write codes main_imgbtn_1
    ui->main_imgbtn_1 = lv_imagebutton_create(ui->main_menu_1_subpage_2_cont);
    lv_obj_set_pos(ui->main_imgbtn_1, 40, 40);
    lv_obj_set_size(ui->main_imgbtn_1, 64, 64);
    lv_imagebutton_set_src(ui->main_imgbtn_1, LV_IMAGEBUTTON_STATE_RELEASED, &_folder_RGB888_64x64_RLE, NULL, NULL);
    ui->main_imgbtn_1_label = lv_label_create(ui->main_imgbtn_1);
    lv_label_set_text(ui->main_imgbtn_1_label, "");
    lv_label_set_long_mode(ui->main_imgbtn_1_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->main_imgbtn_1_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->main_imgbtn_1, 0, LV_STATE_DEFAULT);

    //Write style for main_imgbtn_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->main_imgbtn_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_imgbtn_1, &lv_font_montserratMedium_58, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_imgbtn_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_imgbtn_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_imgbtn_1, Part: LV_PART_MAIN, State: LV_STATE_PRESSED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_1, 0, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_1, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui->main_imgbtn_1, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_font(ui->main_imgbtn_1, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_1, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_1, 0, LV_PART_MAIN|LV_STATE_PRESSED);

    //Write style for main_imgbtn_1, Part: LV_PART_MAIN, State: LV_STATE_CHECKED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_1, 0, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_1, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui->main_imgbtn_1, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_font(ui->main_imgbtn_1, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_1, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_1, 0, LV_PART_MAIN|LV_STATE_CHECKED);

    //Write style for main_imgbtn_1, Part: LV_PART_MAIN, State: LV_IMAGEBUTTON_STATE_RELEASED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_1, 0, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_1, 255, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);

    //Write codes main_imgbtn_2
    ui->main_imgbtn_2 = lv_imagebutton_create(ui->main_menu_1_subpage_2_cont);
    lv_obj_set_pos(ui->main_imgbtn_2, 160, 40);
    lv_obj_set_size(ui->main_imgbtn_2, 64, 64);
    lv_imagebutton_set_src(ui->main_imgbtn_2, LV_IMAGEBUTTON_STATE_RELEASED, &_edit_RGB888_64x64_RLE, NULL, NULL);
    ui->main_imgbtn_2_label = lv_label_create(ui->main_imgbtn_2);
    lv_label_set_text(ui->main_imgbtn_2_label, "");
    lv_label_set_long_mode(ui->main_imgbtn_2_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->main_imgbtn_2_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->main_imgbtn_2, 0, LV_STATE_DEFAULT);

    //Write style for main_imgbtn_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->main_imgbtn_2, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_imgbtn_2, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_imgbtn_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_imgbtn_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_imgbtn_2, Part: LV_PART_MAIN, State: LV_STATE_PRESSED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_2, 0, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_2, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui->main_imgbtn_2, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_font(ui->main_imgbtn_2, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_2, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_2, 0, LV_PART_MAIN|LV_STATE_PRESSED);

    //Write style for main_imgbtn_2, Part: LV_PART_MAIN, State: LV_STATE_CHECKED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_2, 0, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_2, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui->main_imgbtn_2, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_font(ui->main_imgbtn_2, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_2, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_2, 0, LV_PART_MAIN|LV_STATE_CHECKED);

    //Write style for main_imgbtn_2, Part: LV_PART_MAIN, State: LV_IMAGEBUTTON_STATE_RELEASED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_2, 0, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_2, 255, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);

    //Write codes main_imgbtn_3
    ui->main_imgbtn_3 = lv_imagebutton_create(ui->main_menu_1_subpage_2_cont);
    lv_obj_set_pos(ui->main_imgbtn_3, 160, 160);
    lv_obj_set_size(ui->main_imgbtn_3, 64, 64);
    lv_imagebutton_set_src(ui->main_imgbtn_3, LV_IMAGEBUTTON_STATE_RELEASED, &_music_RGB888_64x64_RLE, NULL, NULL);
    ui->main_imgbtn_3_label = lv_label_create(ui->main_imgbtn_3);
    lv_label_set_text(ui->main_imgbtn_3_label, "");
    lv_label_set_long_mode(ui->main_imgbtn_3_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->main_imgbtn_3_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->main_imgbtn_3, 0, LV_STATE_DEFAULT);

    //Write style for main_imgbtn_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->main_imgbtn_3, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_imgbtn_3, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_imgbtn_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_imgbtn_3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_imgbtn_3, Part: LV_PART_MAIN, State: LV_STATE_PRESSED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_3, 0, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_3, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui->main_imgbtn_3, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_font(ui->main_imgbtn_3, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_3, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_3, 0, LV_PART_MAIN|LV_STATE_PRESSED);

    //Write style for main_imgbtn_3, Part: LV_PART_MAIN, State: LV_STATE_CHECKED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_3, 0, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_3, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui->main_imgbtn_3, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_font(ui->main_imgbtn_3, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_3, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_3, 0, LV_PART_MAIN|LV_STATE_CHECKED);

    //Write style for main_imgbtn_3, Part: LV_PART_MAIN, State: LV_IMAGEBUTTON_STATE_RELEASED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_3, 0, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_3, 255, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);

    //Write codes main_btn_backend
    ui->main_btn_backend = lv_imagebutton_create(ui->main_menu_1_subpage_2_cont);
    lv_obj_set_pos(ui->main_btn_backend, 40, 160);
    lv_obj_set_size(ui->main_btn_backend, 64, 64);
    lv_imagebutton_set_src(ui->main_btn_backend, LV_IMAGEBUTTON_STATE_RELEASED, &_backend_RGB888_64x64_RLE, NULL, NULL);
    ui->main_btn_backend_label = lv_label_create(ui->main_btn_backend);
    lv_label_set_text(ui->main_btn_backend_label, "");
    lv_label_set_long_mode(ui->main_btn_backend_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->main_btn_backend_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->main_btn_backend, 0, LV_STATE_DEFAULT);

    //Write style for main_btn_backend, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->main_btn_backend, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_btn_backend, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_btn_backend, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_btn_backend, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_btn_backend, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_btn_backend, Part: LV_PART_MAIN, State: LV_STATE_PRESSED.
    lv_obj_set_style_image_recolor_opa(ui->main_btn_backend, 0, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_image_opa(ui->main_btn_backend, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui->main_btn_backend, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_font(ui->main_btn_backend, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_opa(ui->main_btn_backend, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui->main_btn_backend, 0, LV_PART_MAIN|LV_STATE_PRESSED);

    //Write style for main_btn_backend, Part: LV_PART_MAIN, State: LV_STATE_CHECKED.
    lv_obj_set_style_image_recolor_opa(ui->main_btn_backend, 0, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_image_opa(ui->main_btn_backend, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui->main_btn_backend, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_font(ui->main_btn_backend, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui->main_btn_backend, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(ui->main_btn_backend, 0, LV_PART_MAIN|LV_STATE_CHECKED);

    //Write style for main_btn_backend, Part: LV_PART_MAIN, State: LV_IMAGEBUTTON_STATE_RELEASED.
    lv_obj_set_style_image_recolor_opa(ui->main_btn_backend, 0, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);
    lv_obj_set_style_image_opa(ui->main_btn_backend, 255, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);

    //Write codes main_imgbtn_5
    ui->main_imgbtn_5 = lv_imagebutton_create(ui->main_menu_1_subpage_2_cont);
    lv_obj_set_pos(ui->main_imgbtn_5, 280, 160);
    lv_obj_set_size(ui->main_imgbtn_5, 64, 64);
    lv_imagebutton_set_src(ui->main_imgbtn_5, LV_IMAGEBUTTON_STATE_RELEASED, &_palette_RGB888_64x64_RLE, NULL, NULL);
    ui->main_imgbtn_5_label = lv_label_create(ui->main_imgbtn_5);
    lv_label_set_text(ui->main_imgbtn_5_label, "");
    lv_label_set_long_mode(ui->main_imgbtn_5_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->main_imgbtn_5_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->main_imgbtn_5, 0, LV_STATE_DEFAULT);

    //Write style for main_imgbtn_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->main_imgbtn_5, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_imgbtn_5, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_imgbtn_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_imgbtn_5, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_imgbtn_5, Part: LV_PART_MAIN, State: LV_STATE_PRESSED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_5, 0, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_5, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui->main_imgbtn_5, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_font(ui->main_imgbtn_5, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_5, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_5, 0, LV_PART_MAIN|LV_STATE_PRESSED);

    //Write style for main_imgbtn_5, Part: LV_PART_MAIN, State: LV_STATE_CHECKED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_5, 0, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_5, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui->main_imgbtn_5, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_font(ui->main_imgbtn_5, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_5, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_5, 0, LV_PART_MAIN|LV_STATE_CHECKED);

    //Write style for main_imgbtn_5, Part: LV_PART_MAIN, State: LV_IMAGEBUTTON_STATE_RELEASED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_5, 0, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_5, 255, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);

    //Write codes main_imgbtn_6
    ui->main_imgbtn_6 = lv_imagebutton_create(ui->main_menu_1_subpage_2_cont);
    lv_obj_set_pos(ui->main_imgbtn_6, 400, 160);
    lv_obj_set_size(ui->main_imgbtn_6, 64, 64);
    lv_imagebutton_set_src(ui->main_imgbtn_6, LV_IMAGEBUTTON_STATE_RELEASED, &_end_RGB888_64x64_RLE, NULL, NULL);
    ui->main_imgbtn_6_label = lv_label_create(ui->main_imgbtn_6);
    lv_label_set_text(ui->main_imgbtn_6_label, "");
    lv_label_set_long_mode(ui->main_imgbtn_6_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->main_imgbtn_6_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->main_imgbtn_6, 0, LV_STATE_DEFAULT);

    //Write style for main_imgbtn_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->main_imgbtn_6, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_imgbtn_6, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_imgbtn_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_imgbtn_6, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_imgbtn_6, Part: LV_PART_MAIN, State: LV_STATE_PRESSED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_6, 0, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_6, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui->main_imgbtn_6, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_font(ui->main_imgbtn_6, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_6, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_6, 0, LV_PART_MAIN|LV_STATE_PRESSED);

    //Write style for main_imgbtn_6, Part: LV_PART_MAIN, State: LV_STATE_CHECKED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_6, 0, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_6, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui->main_imgbtn_6, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_font(ui->main_imgbtn_6, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_6, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_6, 0, LV_PART_MAIN|LV_STATE_CHECKED);

    //Write style for main_imgbtn_6, Part: LV_PART_MAIN, State: LV_IMAGEBUTTON_STATE_RELEASED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_6, 0, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_6, 255, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);

    //Write codes main_imgbtn_8
    ui->main_imgbtn_8 = lv_imagebutton_create(ui->main_menu_1_subpage_2_cont);
    lv_obj_set_pos(ui->main_imgbtn_8, 400, 40);
    lv_obj_set_size(ui->main_imgbtn_8, 64, 64);
    lv_imagebutton_set_src(ui->main_imgbtn_8, LV_IMAGEBUTTON_STATE_RELEASED, &_wlan_RGB888_64x64_RLE, NULL, NULL);
    ui->main_imgbtn_8_label = lv_label_create(ui->main_imgbtn_8);
    lv_label_set_text(ui->main_imgbtn_8_label, "");
    lv_label_set_long_mode(ui->main_imgbtn_8_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->main_imgbtn_8_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->main_imgbtn_8, 0, LV_STATE_DEFAULT);

    //Write style for main_imgbtn_8, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->main_imgbtn_8, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_imgbtn_8, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_imgbtn_8, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_imgbtn_8, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_imgbtn_8, Part: LV_PART_MAIN, State: LV_STATE_PRESSED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_8, 0, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_8, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui->main_imgbtn_8, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_font(ui->main_imgbtn_8, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_8, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_8, 0, LV_PART_MAIN|LV_STATE_PRESSED);

    //Write style for main_imgbtn_8, Part: LV_PART_MAIN, State: LV_STATE_CHECKED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_8, 0, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_8, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui->main_imgbtn_8, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_font(ui->main_imgbtn_8, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_8, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_8, 0, LV_PART_MAIN|LV_STATE_CHECKED);

    //Write style for main_imgbtn_8, Part: LV_PART_MAIN, State: LV_IMAGEBUTTON_STATE_RELEASED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_8, 0, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_8, 255, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);

    //Write codes main_imgbtn_7
    ui->main_imgbtn_7 = lv_imagebutton_create(ui->main_menu_1_subpage_2_cont);
    lv_obj_set_pos(ui->main_imgbtn_7, 280, 40);
    lv_obj_set_size(ui->main_imgbtn_7, 64, 64);
    lv_imagebutton_set_src(ui->main_imgbtn_7, LV_IMAGEBUTTON_STATE_RELEASED, &_image_RGB888_64x64_RLE, NULL, NULL);
    ui->main_imgbtn_7_label = lv_label_create(ui->main_imgbtn_7);
    lv_label_set_text(ui->main_imgbtn_7_label, "");
    lv_label_set_long_mode(ui->main_imgbtn_7_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->main_imgbtn_7_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->main_imgbtn_7, 0, LV_STATE_DEFAULT);

    //Write style for main_imgbtn_7, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->main_imgbtn_7, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_imgbtn_7, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_imgbtn_7, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_imgbtn_7, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_imgbtn_7, Part: LV_PART_MAIN, State: LV_STATE_PRESSED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_7, 0, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_7, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui->main_imgbtn_7, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_font(ui->main_imgbtn_7, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_7, 255, LV_PART_MAIN|LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_7, 0, LV_PART_MAIN|LV_STATE_PRESSED);

    //Write style for main_imgbtn_7, Part: LV_PART_MAIN, State: LV_STATE_CHECKED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_7, 0, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_7, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui->main_imgbtn_7, lv_color_hex(0xFF33FF), LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_font(ui->main_imgbtn_7, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui->main_imgbtn_7, 255, LV_PART_MAIN|LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(ui->main_imgbtn_7, 0, LV_PART_MAIN|LV_STATE_CHECKED);

    //Write style for main_imgbtn_7, Part: LV_PART_MAIN, State: LV_IMAGEBUTTON_STATE_RELEASED.
    lv_obj_set_style_image_recolor_opa(ui->main_imgbtn_7, 0, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);
    lv_obj_set_style_image_opa(ui->main_imgbtn_7, 255, LV_PART_MAIN|LV_IMAGEBUTTON_STATE_RELEASED);

    //Write codes main_btn_1
    ui->main_btn_1 = lv_button_create(ui->main_menu_1_subpage_2_cont);
    lv_obj_set_pos(ui->main_btn_1, 40, 270);
    lv_obj_set_size(ui->main_btn_1, 100, 50);
    ui->main_btn_1_label = lv_label_create(ui->main_btn_1);
    lv_label_set_text(ui->main_btn_1_label, "2048");
    lv_label_set_long_mode(ui->main_btn_1_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->main_btn_1_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->main_btn_1, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->main_btn_1_label, LV_PCT(100));

    //Write style for main_btn_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->main_btn_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_btn_1, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_btn_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->main_btn_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_btn_1, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_btn_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->main_btn_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_btn_1, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_btn_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_btn_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);



    //Write codes main_tabview_1
    ui->main_tabview_1 = lv_tabview_create(ui->main_menu_1_subpage_3_cont);
    lv_obj_set_pos(ui->main_tabview_1, 15, 15);
    lv_obj_set_size(ui->main_tabview_1, 530, 450);
    lv_obj_set_scrollbar_mode(ui->main_tabview_1, LV_SCROLLBAR_MODE_OFF);
    lv_tabview_set_tab_bar_position(ui->main_tabview_1, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(ui->main_tabview_1, 50);
    ui->main_tabview_1_tab_1 = lv_tabview_add_tab(ui->main_tabview_1, "Display Model");
    ui->main_tabview_1_tab_2 = lv_tabview_add_tab(ui->main_tabview_1, "Keyboard Model");
    ui->main_tabview_1_tab_3 = lv_tabview_add_tab(ui->main_tabview_1, "Power Model");

    //Write style for main_tabview_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->main_tabview_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_tabview_1, lv_color_hex(0xeaeff3), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_tabview_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->main_tabview_1, lv_color_hex(0x4d4d4d), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_tabview_1, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_tabview_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->main_tabview_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->main_tabview_1, 16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->main_tabview_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_tabview_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_tabview_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_main_tabview_1_extra_btnm_main_default
    static lv_style_t style_main_tabview_1_extra_btnm_main_default;
    ui_init_style(&style_main_tabview_1_extra_btnm_main_default);

    lv_style_set_bg_opa(&style_main_tabview_1_extra_btnm_main_default, 255);
    lv_style_set_bg_color(&style_main_tabview_1_extra_btnm_main_default, lv_color_hex(0xffffff));
    lv_style_set_bg_grad_dir(&style_main_tabview_1_extra_btnm_main_default, LV_GRAD_DIR_NONE);
    lv_style_set_border_width(&style_main_tabview_1_extra_btnm_main_default, 0);
    lv_style_set_radius(&style_main_tabview_1_extra_btnm_main_default, 0);
    for(uint32_t i = 0; i < lv_tabview_get_tab_count(ui->main_tabview_1); i++)
    {
        lv_obj_add_style(lv_obj_get_child(lv_tabview_get_tab_bar(ui->main_tabview_1), i), &style_main_tabview_1_extra_btnm_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);
    }

    //Write style state: LV_STATE_DEFAULT for &style_main_tabview_1_extra_btnm_items_default
    static lv_style_t style_main_tabview_1_extra_btnm_items_default;
    ui_init_style(&style_main_tabview_1_extra_btnm_items_default);

    lv_style_set_text_color(&style_main_tabview_1_extra_btnm_items_default, lv_color_hex(0x4d4d4d));
    lv_style_set_text_font(&style_main_tabview_1_extra_btnm_items_default, &lv_font_montserratMedium_12);
    lv_style_set_text_opa(&style_main_tabview_1_extra_btnm_items_default, 255);
    for(uint32_t i = 0; i < lv_tabview_get_tab_count(ui->main_tabview_1); i++)
    {
        lv_obj_add_style(lv_obj_get_child(lv_tabview_get_tab_bar(ui->main_tabview_1), i), &style_main_tabview_1_extra_btnm_items_default, LV_PART_MAIN|LV_STATE_DEFAULT);
    }

    //Write style state: LV_STATE_CHECKED for &style_main_tabview_1_extra_btnm_items_checked
    static lv_style_t style_main_tabview_1_extra_btnm_items_checked;
    ui_init_style(&style_main_tabview_1_extra_btnm_items_checked);

    lv_style_set_text_color(&style_main_tabview_1_extra_btnm_items_checked, lv_color_hex(0x2195f6));
    lv_style_set_text_font(&style_main_tabview_1_extra_btnm_items_checked, &lv_font_montserratMedium_12);
    lv_style_set_text_opa(&style_main_tabview_1_extra_btnm_items_checked, 255);
    lv_style_set_border_width(&style_main_tabview_1_extra_btnm_items_checked, 4);
    lv_style_set_border_opa(&style_main_tabview_1_extra_btnm_items_checked, 255);
    lv_style_set_border_color(&style_main_tabview_1_extra_btnm_items_checked, lv_color_hex(0x2195f6));
    lv_style_set_border_side(&style_main_tabview_1_extra_btnm_items_checked, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_radius(&style_main_tabview_1_extra_btnm_items_checked, 0);
    lv_style_set_bg_opa(&style_main_tabview_1_extra_btnm_items_checked, 60);
    lv_style_set_bg_color(&style_main_tabview_1_extra_btnm_items_checked, lv_color_hex(0x2195f6));
    lv_style_set_bg_grad_dir(&style_main_tabview_1_extra_btnm_items_checked, LV_GRAD_DIR_NONE);
    for(uint32_t i = 0; i < lv_tabview_get_tab_count(ui->main_tabview_1); i++)
    {
        lv_obj_add_style(lv_obj_get_child(lv_tabview_get_tab_bar(ui->main_tabview_1), i), &style_main_tabview_1_extra_btnm_items_checked, LV_PART_MAIN|LV_STATE_CHECKED);
    }

    //Write codes Display Model
    lv_obj_t * main_tabview_1_tab_1_label = lv_label_create(ui->main_tabview_1_tab_1);
    lv_label_set_text(main_tabview_1_tab_1_label, "Online");

    //Write codes Keyboard Model
    lv_obj_t * main_tabview_1_tab_2_label = lv_label_create(ui->main_tabview_1_tab_2);
    lv_label_set_text(main_tabview_1_tab_2_label, "Offline");

    //Write codes Power Model
    lv_obj_t * main_tabview_1_tab_3_label = lv_label_create(ui->main_tabview_1_tab_3);
    lv_label_set_text(main_tabview_1_tab_3_label, "Online");



    //Write codes main_sw_1
    ui->main_sw_1 = lv_switch_create(ui->main_menu_1_subpage_4_cont);
    lv_obj_set_pos(ui->main_sw_1, 204, 45);
    lv_obj_set_size(ui->main_sw_1, 66, 20);

    //Write style for main_sw_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->main_sw_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_sw_1, lv_color_hex(0xe6e2e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_sw_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->main_sw_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_sw_1, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_sw_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_sw_1, Part: LV_PART_INDICATOR, State: LV_STATE_CHECKED.
    lv_obj_set_style_bg_opa(ui->main_sw_1, 255, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(ui->main_sw_1, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_dir(ui->main_sw_1, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_border_width(ui->main_sw_1, 0, LV_PART_INDICATOR|LV_STATE_CHECKED);

    //Write style for main_sw_1, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->main_sw_1, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_sw_1, lv_color_hex(0xffffff), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_sw_1, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->main_sw_1, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_sw_1, 10, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes main_sw_2
    ui->main_sw_2 = lv_switch_create(ui->main_menu_1_subpage_4_cont);
    lv_obj_set_pos(ui->main_sw_2, 202, 93);
    lv_obj_set_size(ui->main_sw_2, 72, 20);

    //Write style for main_sw_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->main_sw_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_sw_2, lv_color_hex(0xe6e2e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_sw_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->main_sw_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_sw_2, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_sw_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_sw_2, Part: LV_PART_INDICATOR, State: LV_STATE_CHECKED.
    lv_obj_set_style_bg_opa(ui->main_sw_2, 255, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(ui->main_sw_2, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_dir(ui->main_sw_2, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_border_width(ui->main_sw_2, 0, LV_PART_INDICATOR|LV_STATE_CHECKED);

    //Write style for main_sw_2, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->main_sw_2, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_sw_2, lv_color_hex(0xffffff), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_sw_2, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->main_sw_2, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_sw_2, 10, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes main_slider_1
    ui->main_slider_1 = lv_slider_create(ui->main_menu_1_subpage_4_cont);
    lv_obj_set_pos(ui->main_slider_1, 199, 141);
    lv_obj_set_size(ui->main_slider_1, 160, 6);
    lv_slider_set_range(ui->main_slider_1, 0, 100);
    lv_slider_set_mode(ui->main_slider_1, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_value(ui->main_slider_1, 50, LV_ANIM_OFF);

    //Write style for main_slider_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->main_slider_1, 60, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_slider_1, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_slider_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_slider_1, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui->main_slider_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_slider_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_slider_1, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->main_slider_1, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_slider_1, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_slider_1, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_slider_1, 8, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for main_slider_1, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->main_slider_1, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_slider_1, lv_color_hex(0x2195f6), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_slider_1, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_slider_1, 8, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes main_cb_1
    ui->main_cb_1 = lv_checkbox_create(ui->main_menu_1_subpage_4_cont);
    lv_obj_set_pos(ui->main_cb_1, 201, 178);
    lv_checkbox_set_text(ui->main_cb_1, "checkbox");

    //Write style for main_cb_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_pad_top(ui->main_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->main_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->main_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->main_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->main_cb_1, lv_color_hex(0x0D3055), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_cb_1, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_cb_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->main_cb_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->main_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_cb_1, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->main_cb_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_cb_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_cb_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for main_cb_1, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_pad_all(ui->main_cb_1, 3, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->main_cb_1, 2, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->main_cb_1, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->main_cb_1, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->main_cb_1, LV_BORDER_SIDE_FULL, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_cb_1, 6, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->main_cb_1, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_cb_1, lv_color_hex(0xffffff), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_cb_1, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write codes main_ddlist_1
    ui->main_ddlist_1 = lv_dropdown_create(ui->main_menu_1_subpage_4_cont);
    lv_obj_set_pos(ui->main_ddlist_1, 201, 219);
    lv_obj_set_size(ui->main_ddlist_1, 130, 30);
    lv_dropdown_set_options(ui->main_ddlist_1, "ELAB\nSratle\nKawakaze");

    //Write style for main_ddlist_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->main_ddlist_1, lv_color_hex(0x0D3055), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_ddlist_1, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_ddlist_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->main_ddlist_1, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->main_ddlist_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->main_ddlist_1, lv_color_hex(0xe1e6ee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->main_ddlist_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->main_ddlist_1, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->main_ddlist_1, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->main_ddlist_1, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_ddlist_1, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->main_ddlist_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->main_ddlist_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->main_ddlist_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_ddlist_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_CHECKED for &style_main_ddlist_1_extra_list_selected_checked
    static lv_style_t style_main_ddlist_1_extra_list_selected_checked;
    ui_init_style(&style_main_ddlist_1_extra_list_selected_checked);

    lv_style_set_border_width(&style_main_ddlist_1_extra_list_selected_checked, 1);
    lv_style_set_border_opa(&style_main_ddlist_1_extra_list_selected_checked, 255);
    lv_style_set_border_color(&style_main_ddlist_1_extra_list_selected_checked, lv_color_hex(0xe1e6ee));
    lv_style_set_border_side(&style_main_ddlist_1_extra_list_selected_checked, LV_BORDER_SIDE_FULL);
    lv_style_set_radius(&style_main_ddlist_1_extra_list_selected_checked, 3);
    lv_style_set_bg_opa(&style_main_ddlist_1_extra_list_selected_checked, 255);
    lv_style_set_bg_color(&style_main_ddlist_1_extra_list_selected_checked, lv_color_hex(0x00a1b5));
    lv_style_set_bg_grad_dir(&style_main_ddlist_1_extra_list_selected_checked, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->main_ddlist_1), &style_main_ddlist_1_extra_list_selected_checked, LV_PART_SELECTED|LV_STATE_CHECKED);

    //Write style state: LV_STATE_DEFAULT for &style_main_ddlist_1_extra_list_main_default
    static lv_style_t style_main_ddlist_1_extra_list_main_default;
    ui_init_style(&style_main_ddlist_1_extra_list_main_default);

    lv_style_set_max_height(&style_main_ddlist_1_extra_list_main_default, 90);
    lv_style_set_text_color(&style_main_ddlist_1_extra_list_main_default, lv_color_hex(0x0D3055));
    lv_style_set_text_font(&style_main_ddlist_1_extra_list_main_default, &lv_font_montserratMedium_12);
    lv_style_set_text_opa(&style_main_ddlist_1_extra_list_main_default, 255);
    lv_style_set_border_width(&style_main_ddlist_1_extra_list_main_default, 1);
    lv_style_set_border_opa(&style_main_ddlist_1_extra_list_main_default, 255);
    lv_style_set_border_color(&style_main_ddlist_1_extra_list_main_default, lv_color_hex(0xe1e6ee));
    lv_style_set_border_side(&style_main_ddlist_1_extra_list_main_default, LV_BORDER_SIDE_FULL);
    lv_style_set_radius(&style_main_ddlist_1_extra_list_main_default, 3);
    lv_style_set_bg_opa(&style_main_ddlist_1_extra_list_main_default, 255);
    lv_style_set_bg_color(&style_main_ddlist_1_extra_list_main_default, lv_color_hex(0xffffff));
    lv_style_set_bg_grad_dir(&style_main_ddlist_1_extra_list_main_default, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->main_ddlist_1), &style_main_ddlist_1_extra_list_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_main_ddlist_1_extra_list_scrollbar_default
    static lv_style_t style_main_ddlist_1_extra_list_scrollbar_default;
    ui_init_style(&style_main_ddlist_1_extra_list_scrollbar_default);

    lv_style_set_radius(&style_main_ddlist_1_extra_list_scrollbar_default, 3);
    lv_style_set_bg_opa(&style_main_ddlist_1_extra_list_scrollbar_default, 255);
    lv_style_set_bg_color(&style_main_ddlist_1_extra_list_scrollbar_default, lv_color_hex(0x00ff00));
    lv_style_set_bg_grad_dir(&style_main_ddlist_1_extra_list_scrollbar_default, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->main_ddlist_1), &style_main_ddlist_1_extra_list_scrollbar_default, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);

    //Write codes main_label_1
    ui->main_label_1 = lv_label_create(ui->main_menu_1_subpage_4_cont);
    lv_obj_set_pos(ui->main_label_1, 40, 138);
    lv_obj_set_size(ui->main_label_1, 134, 21);
    lv_label_set_text(ui->main_label_1, "Sound Volume");
    lv_label_set_long_mode(ui->main_label_1, LV_LABEL_LONG_WRAP);

    //Write style for main_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->main_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->main_label_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_label_1, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->main_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->main_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_label_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->main_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->main_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->main_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->main_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->main_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes main_label_2
    ui->main_label_2 = lv_label_create(ui->main_menu_1_subpage_4_cont);
    lv_obj_set_pos(ui->main_label_2, 39, 95);
    lv_obj_set_size(ui->main_label_2, 129, 17);
    lv_label_set_text(ui->main_label_2, " Power Reduce");
    lv_label_set_long_mode(ui->main_label_2, LV_LABEL_LONG_WRAP);

    //Write style for main_label_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->main_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->main_label_2, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_label_2, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_label_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->main_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->main_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_label_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->main_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->main_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->main_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->main_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->main_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes main_label_3
    ui->main_label_3 = lv_label_create(ui->main_menu_1_subpage_4_cont);
    lv_obj_set_pos(ui->main_label_3, 32, 48);
    lv_obj_set_size(ui->main_label_3, 150, 16);
    lv_label_set_text(ui->main_label_3, "Auto Secreen-off");
    lv_label_set_long_mode(ui->main_label_3, LV_LABEL_LONG_WRAP);

    //Write style for main_label_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->main_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->main_label_3, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_label_3, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_label_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->main_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->main_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_label_3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->main_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->main_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->main_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->main_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->main_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes main_label_4
    ui->main_label_4 = lv_label_create(ui->main_menu_1_subpage_4_cont);
    lv_obj_set_pos(ui->main_label_4, 57, 180);
    lv_obj_set_size(ui->main_label_4, 100, 19);
    lv_label_set_text(ui->main_label_4, "Debug Info");
    lv_label_set_long_mode(ui->main_label_4, LV_LABEL_LONG_WRAP);

    //Write style for main_label_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->main_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->main_label_4, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_label_4, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_label_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->main_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->main_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_label_4, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->main_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->main_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->main_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->main_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->main_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes main_label_5
    ui->main_label_5 = lv_label_create(ui->main_menu_1_subpage_4_cont);
    lv_obj_set_pos(ui->main_label_5, 37, 225);
    lv_obj_set_size(ui->main_label_5, 133, 18);
    lv_label_set_text(ui->main_label_5, "Device Name");
    lv_label_set_long_mode(ui->main_label_5, LV_LABEL_LONG_WRAP);

    //Write style for main_label_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->main_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->main_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->main_label_5, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->main_label_5, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->main_label_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->main_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->main_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->main_label_5, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->main_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->main_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->main_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->main_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->main_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->main_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of main.


    //Update current screen layout.
    lv_obj_update_layout(ui->main);

    //Init events for screen.
    events_init_main(ui);
}
