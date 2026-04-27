#ifndef TEST_H
#define TEST_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"
#include "../all_devices.h"

extern power_t power_g;
extern keyboard_t keyboard_g;
extern ch9350_t ch9350_g;
extern submodels_t submodels_g[3];

extern cs43131_t CS43131_g;
extern ch378_t ch378_g;
extern ch585f_t ch585f_g;
extern display_t display_g;

void test_CH378(void);

#endif
