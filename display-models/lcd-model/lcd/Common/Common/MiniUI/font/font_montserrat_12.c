/********************************** (C) COPYRIGHT *******************************
* File Name          : font_montserrat_12.c
* Description        : MiniUI font 12px extracted from LVGL.
*                      ASCII 32-126 + degree + bullet. 1bpp format.
********************************************************************************/
#include "font_montserrat_12.h"

/* U+0021 '!' */
static const uint8_t glyph_u0021_bitmap[] = {
    0x22, 0x29, 0x15, 0x40, 
};

/* U+0022 '"' */
static const uint8_t glyph_u0022_bitmap[] = {
    0x61, 0x7F, 
};

/* U+0023 '#' */
static const uint8_t glyph_u0023_bitmap[] = {
    0x0A, 0x2F, 0xF6, 0x92, 0xF2, 0x84, 0xEF, 0xB2, 0xAF, 0xC8, 0x80, 
};

/* U+0024 '$' */
static const uint8_t glyph_u0024_bitmap[] = {
    0x00, 0x88, 0x49, 0x6A, 0x09, 0x62, 0x6E, 0x69, 0xCE, 0x25, 0x96, 0x80, 
};

/* U+0025 '%' */
static const uint8_t glyph_u0025_bitmap[] = {
    0xCE, 0x55, 0x09, 0xA2, 0x29, 0x86, 0x19, 0x99, 0x26, 0xED, 0xAB, 0xC0, 
};

/* U+0026 '&' */
static const uint8_t glyph_u0026_bitmap[] = {
    0x92, 0x69, 0xC6, 0x2A, 0x06, 0xA0, 0x7E, 0x64, 0x99, 0x99, 0x29, 0x80, 
};

/* U+0027 ''' */
static const uint8_t glyph_u0027_bitmap[] = {
    0xFE, 
};

/* U+0028 '(' */
static const uint8_t glyph_u0028_bitmap[] = {
    0x2A, 0xAA, 0x9E, 0x99, 0x2A, 
};

/* U+0029 ')' */
static const uint8_t glyph_u0029_bitmap[] = {
    0xF0, 0xFC, 0xFC, 0x6A, 0x78, 
};

/* U+002A '*' */
static const uint8_t glyph_u002A_bitmap[] = {
    0x99, 0xEE, 0x4E, 0x00, 
};

/* U+002B '+' */
static const uint8_t glyph_u002B_bitmap[] = {
    0x43, 0xFC, 0x10, 0xF8, 0x0F, 0x00, 
};

/* U+002C ',' */
static const uint8_t glyph_u002C_bitmap[] = {
    0x32, 0x60, 
};

/* U+002D '-' */
static const uint8_t glyph_u002D_bitmap[] = {
    0x67, 
};

/* U+002E '.' */
static const uint8_t glyph_u002E_bitmap[] = {
    0x9C, 
};

/* U+002F '/' */
static const uint8_t glyph_u002F_bitmap[] = {
    0x20, 0x45, 0xC6, 0x64, 0x92, 0x9B, 0xCF, 0xF8, 0x84, 0xA4, 
};

/* U+0030 '0' */
static const uint8_t glyph_u0030_bitmap[] = {
    0x2F, 0xC9, 0x92, 0x1B, 0xC4, 0x66, 0x46, 0xF3, 0xA2, 
};

/* U+0031 '1' */
static const uint8_t glyph_u0031_bitmap[] = {
    0xEB, 0xF8, 0xC1, 0x9E, 0x60, 
};

/* U+0032 '2' */
static const uint8_t glyph_u0032_bitmap[] = {
    0x96, 0x92, 0x46, 0xCF, 0x19, 0xF3, 0xE0, 0x30, 
};

/* U+0033 '3' */
static const uint8_t glyph_u0033_bitmap[] = {
    0xA6, 0xC7, 0xA6, 0x6A, 0x9F, 0xB1, 0x2F, 0xA8, 
};

/* U+0034 '4' */
static const uint8_t glyph_u0034_bitmap[] = {
    0x84, 0x94, 0x92, 0x59, 0x8F, 0x6B, 0x0F, 0x13, 0xAA, 
};

/* U+0035 '5' */
static const uint8_t glyph_u0035_bitmap[] = {
    0x91, 0xD3, 0xFA, 0x08, 0xFF, 0x21, 0xAA, 0xBE, 
};

/* U+0036 '6' */
static const uint8_t glyph_u0036_bitmap[] = {
    0x30, 0xAA, 0x0B, 0xF1, 0xE6, 0x62, 0x61, 0x99, 0xEF, 
};

/* U+0037 '7' */
static const uint8_t glyph_u0037_bitmap[] = {
    0xC8, 0xC9, 0x29, 0x29, 0x8C, 0xFC, 0xFD, 0x82, 
};

/* U+0038 '8' */
static const uint8_t glyph_u0038_bitmap[] = {
    0x3F, 0x82, 0x83, 0xF3, 0xF6, 0x08, 0xFE, 0x08, 0x9E, 
};

/* U+0039 '9' */
static const uint8_t glyph_u0039_bitmap[] = {
    0x66, 0x24, 0xA0, 0x62, 0x7A, 0xAA, 0xAB, 0xFA, 
};

/* U+003A ':' */
static const uint8_t glyph_u003A_bitmap[] = {
    0x0A, 0xAA, 0xA8, 
};

/* U+003B ';' */
static const uint8_t glyph_u003B_bitmap[] = {
    0xAA, 0xAB, 0xC5, 0x40, 
};

/* U+003C '<' */
static const uint8_t glyph_u003C_bitmap[] = {
    0x53, 0xEA, 0x92, 0xA4, 0xF3, 0xEF, 0x80, 
};

/* U+003D '=' */
static const uint8_t glyph_u003D_bitmap[] = {
    0x4A, 0x88, 0x8A, 0x0F, 0xE0, 
};

/* U+003E '>' */
static const uint8_t glyph_u003E_bitmap[] = {
    0xB2, 0xE6, 0xAA, 0x9A, 0x92, 0xBA, 0x80, 
};

/* U+003F '?' */
static const uint8_t glyph_u003F_bitmap[] = {
    0xAB, 0xB2, 0xEA, 0xEA, 0xAA, 0x9A, 0xA9, 0xEC, 
};

/* U+0040 '@' */
static const uint8_t glyph_u0040_bitmap[] = {
    0xC2, 0x66, 0x62, 0xCC, 0x7B, 0xFC, 0x6A, 0xA9, 0xBF, 0x32, 0x9E, 0xCC, 
    0x26, 0x66, 0x2C, 0x47, 0xA7, 0xBF, 
};

/* U+0041 'A' */
static const uint8_t glyph_u0041_bitmap[] = {
    0xC6, 0xAA, 0x9B, 0xF3, 0x39, 0x2B, 0xC8, 0x1C, 0xEE, 0x8F, 0xCF, 0xC0, 
};

/* U+0042 'B' */
static const uint8_t glyph_u0042_bitmap[] = {
    0x88, 0x49, 0x24, 0x92, 0xAA, 0xAA, 0xAB, 0x99, 0xBC, 
};

/* U+0043 'C' */
static const uint8_t glyph_u0043_bitmap[] = {
    0xAA, 0xB4, 0x46, 0xAB, 0x31, 0xE4, 0xE3, 0x09, 0x19, 0x9B, 0x80, 
};

/* U+0044 'D' */
static const uint8_t glyph_u0044_bitmap[] = {
    0xBA, 0x99, 0xA4, 0x4A, 0xA4, 0x69, 0xE4, 0xAF, 0xA4, 0xBA, 0x00, 
};

/* U+0045 'E' */
static const uint8_t glyph_u0045_bitmap[] = {
    0xCA, 0x27, 0xE1, 0x92, 0xB9, 0x81, 0xFF, 0xC8, 
};

/* U+0046 'F' */
static const uint8_t glyph_u0046_bitmap[] = {
    0x88, 0x88, 0xE1, 0x2A, 0x90, 0xAB, 0x99, 0x98, 
};

/* U+0047 'G' */
static const uint8_t glyph_u0047_bitmap[] = {
    0x99, 0xEA, 0x90, 0x6B, 0xF0, 0xBA, 0x64, 0xF9, 0x0A, 0xF2, 0x00, 
};

/* U+0048 'H' */
static const uint8_t glyph_u0048_bitmap[] = {
    0x9C, 0xCD, 0x3B, 0x3C, 0xD7, 0x3F, 0x26, 0x72, 0xFA, 
};

/* U+0049 'I' */
static const uint8_t glyph_u0049_bitmap[] = {
    0xBA, 0x99, 0x80, 
};

/* U+004A 'J' */
static const uint8_t glyph_u004A_bitmap[] = {
    0x66, 0x9B, 0xAE, 0xC9, 0xBF, 0x72, 0xFC, 
};

/* U+004B 'K' */
static const uint8_t glyph_u004B_bitmap[] = {
    0x30, 0xE2, 0x22, 0xEA, 0x66, 0x66, 0x9B, 0xA8, 0x6F, 
};

/* U+004C 'L' */
static const uint8_t glyph_u004C_bitmap[] = {
    0x22, 0x73, 0x32, 0x92, 0x92, 0x28, 0x88, 0x82, 
};

/* U+004D 'M' */
static const uint8_t glyph_u004D_bitmap[] = {
    0x22, 0x20, 0xB8, 0x8A, 0x9B, 0xEE, 0x8A, 0xAA, 0xAA, 0xAE, 0xF9, 0xC0, 
};

/* U+004E 'N' */
static const uint8_t glyph_u004E_bitmap[] = {
    0x29, 0x32, 0xE7, 0x2E, 0x6B, 0xA6, 0x92, 0x92, 0x8F, 
};

/* U+004F 'O' */
static const uint8_t glyph_u004F_bitmap[] = {
    0x99, 0x99, 0xA6, 0xFB, 0xA2, 0xB3, 0xB9, 0xAE, 0x23, 0xA9, 0x99, 0x80, 
};

/* U+0050 'P' */
static const uint8_t glyph_u0050_bitmap[] = {
    0x69, 0xBA, 0xAA, 0xF2, 0xAA, 0xE4, 0x13, 0xB0, 0xDE, 
};

/* U+0051 'Q' */
static const uint8_t glyph_u0051_bitmap[] = {
    0x2F, 0x22, 0x33, 0x24, 0xA4, 0xA6, 0x67, 0x4E, 0xAB, 0x17, 0xA4, 0xAA, 
    0x69, 0x96, 0xCA, 
};

/* U+0052 'R' */
static const uint8_t glyph_u0052_bitmap[] = {
    0x6F, 0x94, 0xD3, 0xAE, 0x85, 0x0E, 0xAA, 0x17, 0xA4, 
};

/* U+0053 'S' */
static const uint8_t glyph_u0053_bitmap[] = {
    0x77, 0xE3, 0x93, 0xFE, 0xAA, 0x4A, 0xAB, 0xAA, 
};

/* U+0054 'T' */
static const uint8_t glyph_u0054_bitmap[] = {
    0xAA, 0xAA, 0xB0, 0xAA, 0xAA, 0xAC, 0xCB, 0xAA, 
};

/* U+0055 'U' */
static const uint8_t glyph_u0055_bitmap[] = {
    0x0A, 0x8E, 0x1D, 0xFF, 0xFF, 0xEE, 0x3B, 0xB9, 0xDB, 
};

/* U+0056 'V' */
static const uint8_t glyph_u0056_bitmap[] = {
    0xFE, 0xFF, 0x06, 0xBF, 0xDE, 0x07, 0xA7, 0x81, 0xEF, 0xFE, 0x07, 0x80, 
};

/* U+0057 'W' */
static const uint8_t glyph_u0057_bitmap[] = {
    0x9E, 0x07, 0xBF, 0xDE, 0xFF, 0xFF, 0xFE, 0xFF, 0x00, 0x3B, 0xFF, 0xFF, 
    0xFB, 0xFC, 0x00, 0xEC, 
};

/* U+0058 'X' */
static const uint8_t glyph_u0058_bitmap[] = {
    0xFF, 0xFF, 0xFB, 0xFE, 0x7E, 0x47, 0x3B, 0x9D, 0xCF, 
};

/* U+0059 'Y' */
static const uint8_t glyph_u0059_bitmap[] = {
    0x9C, 0x82, 0x6E, 0x7F, 0xCF, 0x1E, 0x7E, 0xCE, 0x4C, 0x23, 0x80, 
};

/* U+005A 'Z' */
static const uint8_t glyph_u005A_bitmap[] = {
    0xC6, 0x99, 0xAB, 0x2F, 0x2F, 0x0D, 0xAC, 0xEF, 0xF3, 
};

/* U+005B '[' */
static const uint8_t glyph_u005B_bitmap[] = {
    0xE0, 0x18, 0xF0, 0xFF, 0x9E, 
};

/* U+005C '\' */
static const uint8_t glyph_u005C_bitmap[] = {
    0xF9, 0xCE, 0xCC, 0x73, 0x9F, 0xFB, 0xFE, 0x3C, 0x18, 0x24, 
};

/* U+005D ']' */
static const uint8_t glyph_u005D_bitmap[] = {
    0xCB, 0x79, 0xA6, 0xDE, 0xCE, 
};

/* U+005E '^' */
static const uint8_t glyph_u005E_bitmap[] = {
    0xFD, 0x87, 0xFA, 0xFF, 0xBF, 0x80, 
};

/* U+005F '_' */
static const uint8_t glyph_u005F_bitmap[] = {
    0xFC, 
};

/* U+0060 '`' */
static const uint8_t glyph_u0060_bitmap[] = {
    0xBF, 
};

/* U+0061 'a' */
static const uint8_t glyph_u0061_bitmap[] = {
    0x81, 0xBF, 0xFF, 0xFD, 0xFB, 0xC0, 0x00, 
};

/* U+0062 'b' */
static const uint8_t glyph_u0062_bitmap[] = {
    0x83, 0xE7, 0xFF, 0xFF, 0xFD, 0xFF, 0xFD, 0xFE, 0xFC, 
};

/* U+0063 'c' */
static const uint8_t glyph_u0063_bitmap[] = {
    0xFB, 0x11, 0xBC, 0x7B, 0xEF, 0xFF, 0x80, 
};

/* U+0064 'd' */
static const uint8_t glyph_u0064_bitmap[] = {
    0xFF, 0xFF, 0xEF, 0xFF, 0x85, 0xF3, 0xFF, 0xE7, 0x47, 0xD7, 
};

/* U+0065 'e' */
static const uint8_t glyph_u0065_bitmap[] = {
    0xFE, 0xBE, 0xB9, 0xBF, 0xF3, 0xFA, 0x00, 
};

/* U+0066 'f' */
static const uint8_t glyph_u0066_bitmap[] = {
    0x77, 0xFF, 0xFF, 0xFF, 0xF5, 0x67, 0xC0, 
};

/* U+0067 'g' */
static const uint8_t glyph_u0067_bitmap[] = {
    0xFB, 0xFF, 0xFB, 0xFF, 0xB2, 0x4C, 0xD6, 0x7E, 0xBF, 0xAB, 
};

/* U+0068 'h' */
static const uint8_t glyph_u0068_bitmap[] = {
    0xFA, 0xBF, 0xAB, 0xFA, 0xD6, 0x4C, 0xBF, 0xFF, 0x7C, 
};

/* U+0069 'i' */
static const uint8_t glyph_u0069_bitmap[] = {
    0xFB, 0xFF, 0x79, 0xFC, 
};

/* U+006A 'j' */
static const uint8_t glyph_u006A_bitmap[] = {
    0xCF, 0x2F, 0xFF, 0xFF, 0xFF, 0xCC, 0x6E, 0xFB, 0x80, 
};

/* U+006B 'k' */
static const uint8_t glyph_u006B_bitmap[] = {
    0xCF, 0xFF, 0xEF, 0xF6, 0xF9, 0xBF, 0xF0, 0x4C, 0xF0, 
};

/* U+006C 'l' */
static const uint8_t glyph_u006C_bitmap[] = {
    0x7C, 0xFF, 0xF0, 
};

/* U+006D 'm' */
static const uint8_t glyph_u006D_bitmap[] = {
    0xFF, 0xFF, 0xDF, 0xCF, 0xCF, 0xCC, 0xFB, 0xEF, 0xF3, 0xF8, 
};

/* U+006E 'n' */
static const uint8_t glyph_u006E_bitmap[] = {
    0xBF, 0xF7, 0xFE, 0xFF, 0x3E, 0xFB, 0x00, 
};

/* U+006F 'o' */
static const uint8_t glyph_u006F_bitmap[] = {
    0x3C, 0xF7, 0xBB, 0xEF, 0xFF, 0xFE, 0xFF, 
};

/* U+0070 'p' */
static const uint8_t glyph_u0070_bitmap[] = {
    0xFF, 0xEF, 0xFF, 0xFE, 0xFF, 0xB9, 0xC7, 0x1F, 0xF4, 
};

/* U+0071 'q' */
static const uint8_t glyph_u0071_bitmap[] = {
    0xFF, 0xBF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
};

/* U+0072 'r' */
static const uint8_t glyph_u0072_bitmap[] = {
    0xFF, 0xFF, 0xBF, 0xF0, 
};

/* U+0073 's' */
static const uint8_t glyph_u0073_bitmap[] = {
    0x05, 0xBF, 0xFE, 0xFF, 0xFF, 0xC0, 
};

/* U+0074 't' */
static const uint8_t glyph_u0074_bitmap[] = {
    0xFF, 0xFB, 0xF3, 0xED, 0x31, 0xF8, 
};

/* U+0075 'u' */
static const uint8_t glyph_u0075_bitmap[] = {
    0x9F, 0xFF, 0xCF, 0xFB, 0xFE, 0x00, 0x00, 
};

/* U+0076 'v' */
static const uint8_t glyph_u0076_bitmap[] = {
    0xFF, 0xBF, 0xFF, 0xF8, 0xF3, 0x33, 0x33, 
};

/* U+0077 'w' */
static const uint8_t glyph_u0077_bitmap[] = {
    0x33, 0x0C, 0xCC, 0xCF, 0x33, 0x30, 0x4B, 0x27, 0xFD, 0xF8, 
};

/* U+0078 'x' */
static const uint8_t glyph_u0078_bitmap[] = {
    0xEF, 0xFC, 0xB2, 0xC0, 0x01, 0xFF, 0x80, 
};

/* U+0079 'y' */
static const uint8_t glyph_u0079_bitmap[] = {
    0xFF, 0xCF, 0xB8, 0xEC, 0x31, 0xFB, 0xB9, 0xEE, 0xF7, 0xBB, 
};

/* U+007A 'z' */
static const uint8_t glyph_u007A_bitmap[] = {
    0x37, 0x38, 0xEF, 0x8C, 0x77, 0xC0, 
};

/* U+007B '{' */
static const uint8_t glyph_u007B_bitmap[] = {
    0x3C, 0x73, 0x4E, 0x9E, 0x77, 0xF7, 0x30, 
};

/* U+007C '|' */
static const uint8_t glyph_u007C_bitmap[] = {
    0x37, 0xE7, 0x9C, 0xC0, 
};

/* U+007D '}' */
static const uint8_t glyph_u007D_bitmap[] = {
    0x9E, 0xC6, 0x0F, 0xBE, 0x7E, 0xF3, 0x90, 
};

/* U+007E '~' */
static const uint8_t glyph_u007E_bitmap[] = {
    0xFF, 0x9C, 
};

/* U+00B0 '?' */
static const uint8_t glyph_degree_bitmap[] = {
    0xFE, 0x7F, 0xEF, 0x80, 
};

/* U+2022 '?' */
static const uint8_t glyph_bullet_bitmap[] = {
    0x7C, 0x80, 
};

/* Glyph table */
static const ui_glyph_t montserrat_12_glyphs[] = {
    {32, 0, 0, 0, 0, 3, NULL},
    {33, 3, 9, 0, 0, 3, glyph_u0021_bitmap},
    {34, 4, 4, 0, 5, 5, glyph_u0022_bitmap},
    {35, 9, 9, 0, 0, 8, glyph_u0023_bitmap},
    {36, 7, 13, 0, -2, 7, glyph_u0024_bitmap},
    {37, 10, 9, 0, 0, 10, glyph_u0025_bitmap},
    {38, 9, 10, 0, -1, 8, glyph_u0026_bitmap},
    {39, 2, 4, 0, 5, 3, glyph_u0027_bitmap},
    {40, 3, 13, 1, -3, 4, glyph_u0028_bitmap},
    {41, 3, 13, 0, -3, 4, glyph_u0029_bitmap},
    {42, 5, 5, 0, 5, 5, glyph_u002A_bitmap},
    {43, 7, 6, 0, 2, 7, glyph_u002B_bitmap},
    {44, 3, 4, 0, -2, 3, glyph_u002C_bitmap},
    {45, 4, 2, 0, 2, 5, glyph_u002D_bitmap},
    {46, 3, 2, 0, 0, 3, glyph_u002E_bitmap},
    {47, 6, 13, -1, -1, 4, glyph_u002F_bitmap},
    {48, 8, 9, 0, 0, 8, glyph_u0030_bitmap},
    {49, 4, 9, 0, 0, 4, glyph_u0031_bitmap},
    {50, 7, 9, 0, 0, 7, glyph_u0032_bitmap},
    {51, 7, 9, 0, 0, 7, glyph_u0033_bitmap},
    {52, 8, 9, 0, 0, 8, glyph_u0034_bitmap},
    {53, 7, 9, 0, 0, 7, glyph_u0035_bitmap},
    {54, 8, 9, 0, 0, 7, glyph_u0036_bitmap},
    {55, 7, 9, 0, 0, 7, glyph_u0037_bitmap},
    {56, 8, 9, 0, 0, 8, glyph_u0038_bitmap},
    {57, 7, 9, 0, 0, 7, glyph_u0039_bitmap},
    {58, 3, 7, 0, 0, 3, glyph_u003A_bitmap},
    {59, 3, 9, 0, -2, 3, glyph_u003B_bitmap},
    {60, 7, 7, 0, 1, 7, glyph_u003C_bitmap},
    {61, 7, 5, 0, 2, 7, glyph_u003D_bitmap},
    {62, 7, 7, 0, 1, 7, glyph_u003E_bitmap},
    {63, 7, 9, 0, 0, 7, glyph_u003F_bitmap},
    {64, 12, 12, 0, -3, 12, glyph_u0040_bitmap},
    {65, 10, 9, -1, 0, 9, glyph_u0041_bitmap},
    {66, 8, 9, 1, 0, 9, glyph_u0042_bitmap},
    {67, 9, 9, 0, 0, 9, glyph_u0043_bitmap},
    {68, 9, 9, 1, 0, 10, glyph_u0044_bitmap},
    {69, 7, 9, 1, 0, 8, glyph_u0045_bitmap},
    {70, 7, 9, 1, 0, 8, glyph_u0046_bitmap},
    {71, 9, 9, 0, 0, 9, glyph_u0047_bitmap},
    {72, 8, 9, 1, 0, 10, glyph_u0048_bitmap},
    {73, 2, 9, 1, 0, 4, glyph_u0049_bitmap},
    {74, 6, 9, -1, 0, 6, glyph_u004A_bitmap},
    {75, 8, 9, 1, 0, 9, glyph_u004B_bitmap},
    {76, 7, 9, 1, 0, 7, glyph_u004C_bitmap},
    {77, 10, 9, 1, 0, 11, glyph_u004D_bitmap},
    {78, 8, 9, 1, 0, 10, glyph_u004E_bitmap},
    {79, 10, 9, 0, 0, 10, glyph_u004F_bitmap},
    {80, 8, 9, 1, 0, 9, glyph_u0050_bitmap},
    {81, 10, 12, 0, -3, 10, glyph_u0051_bitmap},
    {82, 8, 9, 1, 0, 9, glyph_u0052_bitmap},
    {83, 7, 9, 0, 0, 7, glyph_u0053_bitmap},
    {84, 7, 9, 0, 0, 7, glyph_u0054_bitmap},
    {85, 8, 9, 1, 0, 10, glyph_u0055_bitmap},
    {86, 10, 9, -1, 0, 9, glyph_u0056_bitmap},
    {87, 14, 9, 0, 0, 14, glyph_u0057_bitmap},
    {88, 8, 9, 0, 0, 8, glyph_u0058_bitmap},
    {89, 9, 9, -1, 0, 8, glyph_u0059_bitmap},
    {90, 8, 9, 0, 0, 8, glyph_u005A_bitmap},
    {91, 3, 13, 1, -3, 4, glyph_u005B_bitmap},
    {92, 6, 13, -1, -1, 4, glyph_u005C_bitmap},
    {93, 3, 13, 0, -3, 4, glyph_u005D_bitmap},
    {94, 7, 6, 0, 2, 7, glyph_u005E_bitmap},
    {95, 6, 1, 0, -1, 6, glyph_u005F_bitmap},
    {96, 4, 2, 1, 8, 7, glyph_u0060_bitmap},
    {97, 7, 7, 0, 0, 7, glyph_u0061_bitmap},
    {98, 7, 10, 1, 0, 8, glyph_u0062_bitmap},
    {99, 7, 7, 0, 0, 7, glyph_u0063_bitmap},
    {100, 8, 10, 0, 0, 8, glyph_u0064_bitmap},
    {101, 7, 7, 0, 0, 7, glyph_u0065_bitmap},
    {102, 5, 10, 0, 0, 4, glyph_u0066_bitmap},
    {103, 8, 10, 0, -3, 8, glyph_u0067_bitmap},
    {104, 7, 10, 1, 0, 8, glyph_u0068_bitmap},
    {105, 3, 10, 0, 0, 3, glyph_u0069_bitmap},
    {106, 5, 13, -2, -3, 3, glyph_u006A_bitmap},
    {107, 7, 10, 1, 0, 7, glyph_u006B_bitmap},
    {108, 2, 10, 1, 0, 3, glyph_u006C_bitmap},
    {109, 11, 7, 1, 0, 13, glyph_u006D_bitmap},
    {110, 7, 7, 1, 0, 8, glyph_u006E_bitmap},
    {111, 8, 7, 0, 0, 8, glyph_u006F_bitmap},
    {112, 7, 10, 1, -3, 8, glyph_u0070_bitmap},
    {113, 8, 10, 0, -3, 8, glyph_u0071_bitmap},
    {114, 4, 7, 1, 0, 5, glyph_u0072_bitmap},
    {115, 6, 7, 0, 0, 6, glyph_u0073_bitmap},
    {116, 5, 9, 0, 0, 5, glyph_u0074_bitmap},
    {117, 7, 7, 1, 0, 8, glyph_u0075_bitmap},
    {118, 8, 7, -1, 0, 7, glyph_u0076_bitmap},
    {119, 11, 7, 0, 0, 11, glyph_u0077_bitmap},
    {120, 7, 7, 0, 0, 7, glyph_u0078_bitmap},
    {121, 8, 10, -1, -3, 7, glyph_u0079_bitmap},
    {122, 6, 7, 0, 0, 6, glyph_u007A_bitmap},
    {123, 4, 13, 0, -3, 4, glyph_u007B_bitmap},
    {124, 2, 13, 1, -3, 4, glyph_u007C_bitmap},
    {125, 4, 13, 0, -3, 4, glyph_u007D_bitmap},
    {126, 7, 2, 0, 4, 7, glyph_u007E_bitmap},
    {176, 5, 5, 0, 5, 5, glyph_degree_bitmap},
    {8226, 3, 3, 0, 2, 4, glyph_bullet_bitmap},
};

/* Font descriptor */
const ui_font_t font_montserrat_12 = {
    .glyphs = montserrat_12_glyphs,
    .glyph_count = 97,
    .height = 13,
    .baseline = 9,
    .bpp = 1,
};
