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



void setup_scr_gameTetris(lv_ui *ui)
{
    //Write codes gameTetris
    ui->gameTetris = lv_obj_create(NULL);
    lv_obj_set_size(ui->gameTetris, 800, 480);
    lv_obj_set_scrollbar_mode(ui->gameTetris, LV_SCROLLBAR_MODE_OFF);

    //Write style for gameTetris, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->gameTetris, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris, lv_color_hex(0xFFFBF5), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_btnm_tetris
    ui->gameTetris_btnm_tetris = lv_buttonmatrix_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_btnm_tetris, 160, 11);
    lv_obj_set_size(ui->gameTetris_btnm_tetris, 228, 452);
    static const char *gameTetris_btnm_tetris_text_map[] = {" ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "\n", " ", " ", " ", " ", " ", " ", " ", " ", "",};
    lv_buttonmatrix_set_map(ui->gameTetris_btnm_tetris, gameTetris_btnm_tetris_text_map);

    //Write style for gameTetris_btnm_tetris, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_btnm_tetris, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_btnm_tetris, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_btnm_tetris, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_btnm_tetris, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_btnm_tetris, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui->gameTetris_btnm_tetris, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui->gameTetris_btnm_tetris, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_btnm_tetris, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_btnm_tetris, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_btnm_tetris, lv_color_hex(0xFFF5E4), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_btnm_tetris, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for gameTetris_btnm_tetris, Part: LV_PART_ITEMS, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_btnm_tetris, 0, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_btnm_tetris, lv_color_hex(0xffffff), LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_btnm_tetris, &lv_font_montserratMedium_16, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_btnm_tetris, 255, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_btnm_tetris, 5, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_btnm_tetris, 255, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_btnm_tetris, lv_color_hex(0x8B7355), LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_btnm_tetris, LV_GRAD_DIR_NONE, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_btnm_tetris, 4, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->gameTetris_btnm_tetris, lv_color_hex(0xF5EBE0), LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->gameTetris_btnm_tetris, 255, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->gameTetris_btnm_tetris, 2, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(ui->gameTetris_btnm_tetris, 1, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(ui->gameTetris_btnm_tetris, 2, LV_PART_ITEMS|LV_STATE_DEFAULT);

    //Write codes gameTetris_btnm_next
    ui->gameTetris_btnm_next = lv_buttonmatrix_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_btnm_next, 626, 198);
    lv_obj_set_size(ui->gameTetris_btnm_next, 116, 116);
    static const char *gameTetris_btnm_next_text_map[] = {" ", " ", " ", " ", "\n", " ", " ", " ", " ", "\n", " ", " ", " ", " ", "\n", " ", " ", " ", " ", "",};
    lv_buttonmatrix_set_map(ui->gameTetris_btnm_next, gameTetris_btnm_next_text_map);

    //Write style for gameTetris_btnm_next, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_btnm_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_btnm_next, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_btnm_next, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_btnm_next, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_btnm_next, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui->gameTetris_btnm_next, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui->gameTetris_btnm_next, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_btnm_next, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_btnm_next, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_btnm_next, lv_color_hex(0xFFF5E4), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_btnm_next, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for gameTetris_btnm_next, Part: LV_PART_ITEMS, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_btnm_next, 0, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_btnm_next, lv_color_hex(0xffffff), LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_btnm_next, &lv_font_montserratMedium_16, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_btnm_next, 255, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_btnm_next, 5, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_btnm_next, 255, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_btnm_next, lv_color_hex(0x8B7355), LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_btnm_next, LV_GRAD_DIR_NONE, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_btnm_next, 4, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->gameTetris_btnm_next, lv_color_hex(0xF5EBE0), LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->gameTetris_btnm_next, 255, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->gameTetris_btnm_next, 2, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(ui->gameTetris_btnm_next, 1, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(ui->gameTetris_btnm_next, 2, LV_PART_ITEMS|LV_STATE_DEFAULT);

    //Write codes gameTetris_label_title
    ui->gameTetris_label_title = lv_label_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_label_title, 489, 20);
    lv_obj_set_size(ui->gameTetris_label_title, 197, 62);
    lv_label_set_text(ui->gameTetris_label_title, "Tetris!");
    lv_label_set_long_mode(ui->gameTetris_label_title, LV_LABEL_LONG_WRAP);

    //Write style for gameTetris_label_title, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_label_title, lv_color_hex(0x8B7355), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_label_title, &lv_font_Alatsi_Regular_64, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_label_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->gameTetris_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->gameTetris_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_label_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_label_next
    ui->gameTetris_label_next = lv_label_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_label_next, 463, 221);
    lv_obj_set_size(ui->gameTetris_label_next, 84, 71);
    lv_label_set_text(ui->gameTetris_label_next, "The NEXT");
    lv_label_set_long_mode(ui->gameTetris_label_next, LV_LABEL_LONG_WRAP);

    //Write style for gameTetris_label_next, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_label_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_label_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_label_next, lv_color_hex(0xA89F91), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_label_next, &lv_font_Alatsi_Regular_32, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_label_next, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->gameTetris_label_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->gameTetris_label_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_label_next, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_label_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_label_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_label_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_label_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_label_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_label_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_label_score_title
    ui->gameTetris_label_score_title = lv_label_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_label_score_title, 422, 96);
    lv_obj_set_size(ui->gameTetris_label_score_title, 166, 88);
    lv_label_set_text(ui->gameTetris_label_score_title, "Score");
    lv_label_set_long_mode(ui->gameTetris_label_score_title, LV_LABEL_LONG_WRAP);

    //Write style for gameTetris_label_score_title, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_label_score_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_label_score_title, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_label_score_title, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_label_score_title, &lv_font_Alatsi_Regular_33, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_label_score_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->gameTetris_label_score_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->gameTetris_label_score_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_label_score_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_label_score_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_label_score_title, lv_color_hex(0xBDAC97), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_label_score_title, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_label_score_title, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_label_score_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_label_score_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_label_score_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_label_score_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_label_best_title
    ui->gameTetris_label_best_title = lv_label_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_label_best_title, 603, 96);
    lv_obj_set_size(ui->gameTetris_label_best_title, 166, 88);
    lv_label_set_text(ui->gameTetris_label_best_title, "Best");
    lv_label_set_long_mode(ui->gameTetris_label_best_title, LV_LABEL_LONG_WRAP);

    //Write style for gameTetris_label_best_title, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_label_best_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_label_best_title, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_label_best_title, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_label_best_title, &lv_font_Alatsi_Regular_33, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_label_best_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->gameTetris_label_best_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->gameTetris_label_best_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_label_best_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_label_best_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_label_best_title, lv_color_hex(0xBDAC97), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_label_best_title, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_label_best_title, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_label_best_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_label_best_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_label_best_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_label_best_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_label_score
    ui->gameTetris_label_score = lv_label_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_label_score, 437, 138);
    lv_obj_set_size(ui->gameTetris_label_score, 135, 44);
    lv_label_set_text(ui->gameTetris_label_score, "0");
    lv_label_set_long_mode(ui->gameTetris_label_score, LV_LABEL_LONG_WRAP);

    //Write style for gameTetris_label_score, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_label_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_label_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_label_score, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_label_score, &lv_font_Alatsi_Regular_36, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_label_score, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->gameTetris_label_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->gameTetris_label_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_label_score, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_label_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_label_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_label_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_label_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_label_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_label_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_label_best_score
    ui->gameTetris_label_best_score = lv_label_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_label_best_score, 620, 138);
    lv_obj_set_size(ui->gameTetris_label_best_score, 135, 44);
    lv_label_set_text(ui->gameTetris_label_best_score, "0");
    lv_label_set_long_mode(ui->gameTetris_label_best_score, LV_LABEL_LONG_WRAP);

    //Write style for gameTetris_label_best_score, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_label_best_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_label_best_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_label_best_score, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_label_best_score, &lv_font_Alatsi_Regular_36, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_label_best_score, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->gameTetris_label_best_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->gameTetris_label_best_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_label_best_score, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_label_best_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_label_best_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_label_best_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_label_best_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_label_best_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_label_best_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_btn_right
    ui->gameTetris_btn_right = lv_button_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_btn_right, 644, 399);
    lv_obj_set_size(ui->gameTetris_btn_right, 80, 60);
    ui->gameTetris_btn_right_label = lv_label_create(ui->gameTetris_btn_right);
    lv_label_set_text(ui->gameTetris_btn_right_label, "" LV_SYMBOL_RIGHT "");
    lv_label_set_long_mode(ui->gameTetris_btn_right_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->gameTetris_btn_right_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->gameTetris_btn_right, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->gameTetris_btn_right_label, LV_PCT(100));

    //Write style for gameTetris_btn_right, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->gameTetris_btn_right, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_btn_right, lv_color_hex(0x9C8678), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_btn_right, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->gameTetris_btn_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_btn_right, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_btn_right, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->gameTetris_btn_right, lv_color_hex(0xBDAC97), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->gameTetris_btn_right, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->gameTetris_btn_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(ui->gameTetris_btn_right, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(ui->gameTetris_btn_right, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_btn_right, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_btn_right, &lv_font_montserratMedium_58, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_btn_right, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_btn_right, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_btn_left
    ui->gameTetris_btn_left = lv_button_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_btn_left, 464, 399);
    lv_obj_set_size(ui->gameTetris_btn_left, 80, 60);
    ui->gameTetris_btn_left_label = lv_label_create(ui->gameTetris_btn_left);
    lv_label_set_text(ui->gameTetris_btn_left_label, "" LV_SYMBOL_LEFT "");
    lv_label_set_long_mode(ui->gameTetris_btn_left_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->gameTetris_btn_left_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->gameTetris_btn_left, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->gameTetris_btn_left_label, LV_PCT(100));

    //Write style for gameTetris_btn_left, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->gameTetris_btn_left, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_btn_left, lv_color_hex(0x9C8678), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_btn_left, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->gameTetris_btn_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_btn_left, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_btn_left, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->gameTetris_btn_left, lv_color_hex(0xBDAC97), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->gameTetris_btn_left, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->gameTetris_btn_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(ui->gameTetris_btn_left, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(ui->gameTetris_btn_left, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_btn_left, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_btn_left, &lv_font_montserratMedium_58, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_btn_left, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_btn_left, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_btn_down
    ui->gameTetris_btn_down = lv_button_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_btn_down, 554, 399);
    lv_obj_set_size(ui->gameTetris_btn_down, 80, 60);
    ui->gameTetris_btn_down_label = lv_label_create(ui->gameTetris_btn_down);
    lv_label_set_text(ui->gameTetris_btn_down_label, "" LV_SYMBOL_DOWN "");
    lv_label_set_long_mode(ui->gameTetris_btn_down_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->gameTetris_btn_down_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->gameTetris_btn_down, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->gameTetris_btn_down_label, LV_PCT(100));

    //Write style for gameTetris_btn_down, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->gameTetris_btn_down, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_btn_down, lv_color_hex(0x9C8678), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_btn_down, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->gameTetris_btn_down, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_btn_down, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_btn_down, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->gameTetris_btn_down, lv_color_hex(0xBDAC97), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->gameTetris_btn_down, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->gameTetris_btn_down, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(ui->gameTetris_btn_down, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(ui->gameTetris_btn_down, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_btn_down, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_btn_down, &lv_font_montserratMedium_58, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_btn_down, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_btn_down, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_btn_up
    ui->gameTetris_btn_up = lv_button_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_btn_up, 554, 329);
    lv_obj_set_size(ui->gameTetris_btn_up, 80, 60);
    ui->gameTetris_btn_up_label = lv_label_create(ui->gameTetris_btn_up);
    lv_label_set_text(ui->gameTetris_btn_up_label, "" LV_SYMBOL_UP "");
    lv_label_set_long_mode(ui->gameTetris_btn_up_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->gameTetris_btn_up_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->gameTetris_btn_up, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->gameTetris_btn_up_label, LV_PCT(100));

    //Write style for gameTetris_btn_up, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->gameTetris_btn_up, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_btn_up, lv_color_hex(0x9C8678), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_btn_up, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->gameTetris_btn_up, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_btn_up, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_btn_up, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->gameTetris_btn_up, lv_color_hex(0xBDAC97), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->gameTetris_btn_up, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->gameTetris_btn_up, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(ui->gameTetris_btn_up, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(ui->gameTetris_btn_up, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_btn_up, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_btn_up, &lv_font_montserratMedium_58, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_btn_up, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_btn_up, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_label_stage_title
    ui->gameTetris_label_stage_title = lv_label_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_label_stage_title, 15, 200);
    lv_obj_set_size(ui->gameTetris_label_stage_title, 128, 129);
    lv_label_set_text(ui->gameTetris_label_stage_title, "STAGE");
    lv_label_set_long_mode(ui->gameTetris_label_stage_title, LV_LABEL_LONG_WRAP);

    //Write style for gameTetris_label_stage_title, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_label_stage_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_label_stage_title, 15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_label_stage_title, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_label_stage_title, &lv_font_Alatsi_Regular_33, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_label_stage_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->gameTetris_label_stage_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->gameTetris_label_stage_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_label_stage_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_label_stage_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_label_stage_title, lv_color_hex(0xBDAC97), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_label_stage_title, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_label_stage_title, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_label_stage_title, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_label_stage_title, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_label_stage_title, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_label_stage_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_bar_stage
    ui->gameTetris_bar_stage = lv_bar_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_bar_stage, 20, 305);
    lv_obj_set_size(ui->gameTetris_bar_stage, 117, 19);
    lv_obj_set_style_anim_duration(ui->gameTetris_bar_stage, 1000, 0);
    lv_bar_set_mode(ui->gameTetris_bar_stage, LV_BAR_MODE_NORMAL);
    lv_bar_set_range(ui->gameTetris_bar_stage, 0, 100);
    lv_bar_set_value(ui->gameTetris_bar_stage, 50, LV_ANIM_OFF);

    //Write style for gameTetris_bar_stage, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->gameTetris_bar_stage, 60, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_bar_stage, lv_color_hex(0xA8DAFF), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_bar_stage, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_bar_stage, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_bar_stage, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for gameTetris_bar_stage, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->gameTetris_bar_stage, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_bar_stage, lv_color_hex(0x8B7355), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_bar_stage, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_bar_stage, 10, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write codes gameTetris_label_stage
    ui->gameTetris_label_stage = lv_label_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_label_stage, 29, 247);
    lv_obj_set_size(ui->gameTetris_label_stage, 100, 46);
    lv_label_set_text(ui->gameTetris_label_stage, "1");
    lv_label_set_long_mode(ui->gameTetris_label_stage, LV_LABEL_LONG_WRAP);

    //Write style for gameTetris_label_stage, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_label_stage, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_label_stage, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_label_stage, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_label_stage, &lv_font_Alatsi_Regular_44, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_label_stage, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->gameTetris_label_stage, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->gameTetris_label_stage, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_label_stage, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_label_stage, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_label_stage, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_label_stage, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_label_stage, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_label_stage, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_label_stage, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_btn_start
    ui->gameTetris_btn_start = lv_button_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_btn_start, 15, 96);
    lv_obj_set_size(ui->gameTetris_btn_start, 126, 58);
    ui->gameTetris_btn_start_label = lv_label_create(ui->gameTetris_btn_start);
    lv_label_set_text(ui->gameTetris_btn_start_label, "Start");
    lv_label_set_long_mode(ui->gameTetris_btn_start_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->gameTetris_btn_start_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->gameTetris_btn_start, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->gameTetris_btn_start_label, LV_PCT(100));

    //Write style for gameTetris_btn_start, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->gameTetris_btn_start, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_btn_start, lv_color_hex(0x9C8678), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_btn_start, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->gameTetris_btn_start, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_btn_start, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_btn_start, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->gameTetris_btn_start, lv_color_hex(0xBDAC97), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->gameTetris_btn_start, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->gameTetris_btn_start, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(ui->gameTetris_btn_start, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(ui->gameTetris_btn_start, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_btn_start, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_btn_start, &lv_font_Alatsi_Regular_30, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_btn_start, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_btn_start, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_btn_back
    ui->gameTetris_btn_back = lv_button_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_btn_back, 705, 24);
    lv_obj_set_size(ui->gameTetris_btn_back, 59, 44);
    ui->gameTetris_btn_back_label = lv_label_create(ui->gameTetris_btn_back);
    lv_label_set_text(ui->gameTetris_btn_back_label, "" LV_SYMBOL_CLOSE "");
    lv_label_set_long_mode(ui->gameTetris_btn_back_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->gameTetris_btn_back_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->gameTetris_btn_back, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->gameTetris_btn_back_label, LV_PCT(100));

    //Write style for gameTetris_btn_back, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->gameTetris_btn_back, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_btn_back, lv_color_hex(0x9C8678), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_btn_back, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->gameTetris_btn_back, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_btn_back, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_btn_back, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->gameTetris_btn_back, lv_color_hex(0xBDAC97), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->gameTetris_btn_back, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->gameTetris_btn_back, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(ui->gameTetris_btn_back, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(ui->gameTetris_btn_back, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_btn_back, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_btn_back, &lv_font_montserratMedium_33, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_btn_back, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_btn_back, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_cont_msgbox
    ui->gameTetris_cont_msgbox = lv_obj_create(ui->gameTetris);
    lv_obj_set_pos(ui->gameTetris_cont_msgbox, 0, 0);
    lv_obj_set_size(ui->gameTetris_cont_msgbox, 800, 480);
    lv_obj_set_scrollbar_mode(ui->gameTetris_cont_msgbox, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(ui->gameTetris_cont_msgbox, LV_OBJ_FLAG_HIDDEN);

    //Write style for gameTetris_cont_msgbox, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_cont_msgbox, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->gameTetris_cont_msgbox, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->gameTetris_cont_msgbox, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->gameTetris_cont_msgbox, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_cont_msgbox, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_cont_msgbox, 154, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_cont_msgbox, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_cont_msgbox, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_cont_msgbox, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_cont_msgbox, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_cont_msgbox, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_cont_msgbox, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_cont_msgbox, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_btn_again
    ui->gameTetris_btn_again = lv_button_create(ui->gameTetris_cont_msgbox);
    lv_obj_set_pos(ui->gameTetris_btn_again, 300, 375);
    lv_obj_set_size(ui->gameTetris_btn_again, 200, 70);
    ui->gameTetris_btn_again_label = lv_label_create(ui->gameTetris_btn_again);
    lv_label_set_text(ui->gameTetris_btn_again_label, "Play Again");
    lv_label_set_long_mode(ui->gameTetris_btn_again_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->gameTetris_btn_again_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->gameTetris_btn_again, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->gameTetris_btn_again_label, LV_PCT(100));

    //Write style for gameTetris_btn_again, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->gameTetris_btn_again, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->gameTetris_btn_again, lv_color_hex(0x8B7355), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->gameTetris_btn_again, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->gameTetris_btn_again, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_btn_again, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_btn_again, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_btn_again, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_btn_again, &lv_font_Alatsi_Regular_33, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_btn_again, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_btn_again, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_label_result_text
    ui->gameTetris_label_result_text = lv_label_create(ui->gameTetris_cont_msgbox);
    lv_obj_set_pos(ui->gameTetris_label_result_text, 255, 176);
    lv_obj_set_size(ui->gameTetris_label_result_text, 150, 170);
    lv_label_set_text(ui->gameTetris_label_result_text, "Move:\nScore:\nBest:");
    lv_label_set_long_mode(ui->gameTetris_label_result_text, LV_LABEL_LONG_WRAP);

    //Write style for gameTetris_label_result_text, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_label_result_text, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_label_result_text, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_label_result_text, lv_color_hex(0x766d68), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_label_result_text, &lv_font_Alatsi_Regular_43, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_label_result_text, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->gameTetris_label_result_text, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->gameTetris_label_result_text, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_label_result_text, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_label_result_text, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_label_result_text, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_label_result_text, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_label_result_text, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_label_result_text, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_label_result_text, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_label_result_score
    ui->gameTetris_label_result_score = lv_label_create(ui->gameTetris_cont_msgbox);
    lv_obj_set_pos(ui->gameTetris_label_result_score, 445, 175);
    lv_obj_set_size(ui->gameTetris_label_result_score, 171, 172);
    lv_label_set_text(ui->gameTetris_label_result_score, "45\n999\n1285");
    lv_label_set_long_mode(ui->gameTetris_label_result_score, LV_LABEL_LONG_WRAP);

    //Write style for gameTetris_label_result_score, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_label_result_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_label_result_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_label_result_score, lv_color_hex(0x766d68), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_label_result_score, &lv_font_Alatsi_Regular_43, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_label_result_score, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->gameTetris_label_result_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->gameTetris_label_result_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_label_result_score, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_label_result_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_label_result_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_label_result_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_label_result_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_label_result_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_label_result_score, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes gameTetris_label_result_title
    ui->gameTetris_label_result_title = lv_label_create(ui->gameTetris_cont_msgbox);
    lv_obj_set_pos(ui->gameTetris_label_result_title, 200, 69);
    lv_obj_set_size(ui->gameTetris_label_result_title, 400, 100);
    lv_label_set_text(ui->gameTetris_label_result_title, "Game End");
    lv_label_set_long_mode(ui->gameTetris_label_result_title, LV_LABEL_LONG_WRAP);

    //Write style for gameTetris_label_result_title, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->gameTetris_label_result_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->gameTetris_label_result_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->gameTetris_label_result_title, lv_color_hex(0xF29191), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->gameTetris_label_result_title, &lv_font_Alatsi_Regular_66, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->gameTetris_label_result_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->gameTetris_label_result_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->gameTetris_label_result_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->gameTetris_label_result_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->gameTetris_label_result_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->gameTetris_label_result_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->gameTetris_label_result_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->gameTetris_label_result_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->gameTetris_label_result_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->gameTetris_label_result_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of gameTetris.


    //Update current screen layout.
    lv_obj_update_layout(ui->gameTetris);

    //Init events for screen.
    events_init_gameTetris(ui);
}
