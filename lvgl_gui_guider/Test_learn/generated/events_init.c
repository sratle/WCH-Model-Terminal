/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "events_init.h"
#include <stdio.h>
#include "lvgl.h"

#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREEMASTER
#include "freemaster_client.h"
#endif

#include "custom.h"

static void main_btn_music_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        ui_load_scr_animation(&guider_ui, &guider_ui.game2048, guider_ui.game2048_del, &guider_ui.main_del, setup_scr_game2048, LV_SCR_LOAD_ANIM_FADE_ON, 200, 200, true, true);
        break;
    }
    default:
        break;
    }
}

static void main_btn_play_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        ui_load_scr_animation(&guider_ui, &guider_ui.game2048, guider_ui.game2048_del, &guider_ui.main_del, setup_scr_game2048, LV_SCR_LOAD_ANIM_OVER_LEFT, 200, 200, true, true);
        init_game2048(&guider_ui);
        break;
    }
    default:
        break;
    }
}

static void main_btn_tetris_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        ui_load_scr_animation(&guider_ui, &guider_ui.gameTetris, guider_ui.gameTetris_del, &guider_ui.main_del, setup_scr_gameTetris, LV_SCR_LOAD_ANIM_OVER_LEFT, 200, 200, false, true);
        init_gameTetris(&guider_ui);
        break;
    }
    default:
        break;
    }
}

void events_init_main (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->main_btn_music, main_btn_music_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->main_btn_play, main_btn_play_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->main_btn_tetris, main_btn_tetris_event_handler, LV_EVENT_ALL, ui);
}

static void game2048_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_GESTURE:
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
        switch(dir) {
        case LV_DIR_LEFT:
        {
            lv_indev_wait_release(lv_indev_active());
            check_game2048(&guider_ui,GG_2048_MOVE_LEFT);
            break;
        }
        case LV_DIR_RIGHT:
        {
            lv_indev_wait_release(lv_indev_active());
            check_game2048(&guider_ui, GG_2048_MOVE_RIGHT);
            break;
        }
        case LV_DIR_TOP:
        {
            lv_indev_wait_release(lv_indev_active());
            check_game2048(&guider_ui,GG_2048_MOVE_UP);
            break;
        }
        case LV_DIR_BOTTOM:
        {
            lv_indev_wait_release(lv_indev_active());
            check_game2048(&guider_ui,GG_2048_MOVE_DOWN);
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void game2048_btn_up_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        check_game2048(&guider_ui, GG_2048_MOVE_UP);
        break;
    }
    default:
        break;
    }
}

static void game2048_btn_new_game_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        new_game2048(&guider_ui);
        break;
    }
    default:
        break;
    }
}

static void game2048_btn_left_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        check_game2048(&guider_ui, GG_2048_MOVE_LEFT);
        break;
    }
    default:
        break;
    }
}

static void game2048_btn_down_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        check_game2048(&guider_ui, GG_2048_MOVE_DOWN);
        break;
    }
    default:
        break;
    }
}

static void game2048_btn_right_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        check_game2048(&guider_ui, GG_2048_MOVE_RIGHT);
        break;
    }
    default:
        break;
    }
}

static void game2048_btn_back_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        ui_load_scr_animation(&guider_ui, &guider_ui.main, guider_ui.main_del, &guider_ui.game2048_del, setup_scr_main, LV_SCR_LOAD_ANIM_OVER_RIGHT, 200, 200, true, true);
        break;
    }
    default:
        break;
    }
}

static void game2048_btn_again_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        lv_obj_add_flag(guider_ui.game2048_cont_msgbox, LV_OBJ_FLAG_HIDDEN);
        new_game2048(&guider_ui);
        break;
    }
    default:
        break;
    }
}

void events_init_game2048 (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->game2048, game2048_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->game2048_btn_up, game2048_btn_up_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->game2048_btn_new_game, game2048_btn_new_game_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->game2048_btn_left, game2048_btn_left_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->game2048_btn_down, game2048_btn_down_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->game2048_btn_right, game2048_btn_right_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->game2048_btn_back, game2048_btn_back_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->game2048_btn_again, game2048_btn_again_event_handler, LV_EVENT_ALL, ui);
}

static void gameTetris_btn_right_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        check_gameTetris(&guider_ui,TETRIS_MOVE_RIGHT);
        break;
    }
    default:
        break;
    }
}

static void gameTetris_btn_left_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        check_gameTetris(&guider_ui,TETRIS_MOVE_LEFT);
        break;
    }
    default:
        break;
    }
}

static void gameTetris_btn_down_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        check_gameTetris(&guider_ui,TETRIS_MOVE_DOWN);
        break;
    }
    default:
        break;
    }
}

static void gameTetris_btn_up_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        check_gameTetris(&guider_ui,TETRIS_MOVE_UP);
        break;
    }
    default:
        break;
    }
}

static void gameTetris_btn_start_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        new_gameTetris(&guider_ui);
        break;
    }
    default:
        break;
    }
}

static void gameTetris_btn_back_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        ui_load_scr_animation(&guider_ui, &guider_ui.main, guider_ui.main_del, &guider_ui.gameTetris_del, setup_scr_main, LV_SCR_LOAD_ANIM_OVER_RIGHT, 200, 200, false, true);
        break;
    }
    default:
        break;
    }
}

static void gameTetris_btn_again_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        new_gameTetris(&guider_ui);
        lv_obj_add_flag(guider_ui.gameTetris_cont_msgbox, LV_OBJ_FLAG_HIDDEN);
        break;
    }
    default:
        break;
    }
}

void events_init_gameTetris (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->gameTetris_btn_right, gameTetris_btn_right_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->gameTetris_btn_left, gameTetris_btn_left_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->gameTetris_btn_down, gameTetris_btn_down_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->gameTetris_btn_up, gameTetris_btn_up_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->gameTetris_btn_start, gameTetris_btn_start_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->gameTetris_btn_back, gameTetris_btn_back_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->gameTetris_btn_again, gameTetris_btn_again_event_handler, LV_EVENT_ALL, ui);
}


void events_init(lv_ui *ui)
{

}
