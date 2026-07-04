/********************************** (C) COPYRIGHT *******************************
* File Name          : font_montserrat_12.c
* Description        : MiniUI font 12px extracted from LVGL.
*                      ASCII 32-126 + degree + bullet. 1bpp format.
********************************************************************************/
#include "font_montserrat_12.h"

/* U+0021 '!' */
static const uint8_t glyph_u0021_bitmap[] = {
    0x49, 0x24, 0x82, 0x40, 
};

/* U+0022 '"' */
static const uint8_t glyph_u0022_bitmap[] = {
    0x55, 0x50, 
};

/* U+0023 '#' */
static const uint8_t glyph_u0023_bitmap[] = {
    0x12, 0x01, 0x1F, 0xC4, 0x82, 0x41, 0x23, 0xFC, 0x48, 0x24, 0x00, 
};

/* U+0024 '$' */
static const uint8_t glyph_u0024_bitmap[] = {
    0x10, 0x20, 0xF2, 0x85, 0x0E, 0x0F, 0x0B, 0x12, 0xAD, 0xF0, 0x80, 0x00, 
};

/* U+0025 '%' */
static const uint8_t glyph_u0025_bitmap[] = {
    0x71, 0x04, 0x08, 0x20, 0x50, 0x65, 0x82, 0x81, 0x24, 0x48, 0x21, 0x80, 
};

/* U+0026 '&' */
static const uint8_t glyph_u0026_bitmap[] = {
    0x38, 0x12, 0x09, 0x07, 0x03, 0x82, 0x59, 0x18, 0x8C, 0x39, 0x00, 0x00, 
};

/* U+0027 ''' */
static const uint8_t glyph_u0027_bitmap[] = {
    0x54, 
};

/* U+0028 '(' */
static const uint8_t glyph_u0028_bitmap[] = {
    0x49, 0x69, 0x24, 0x99, 0x24, 
};

/* U+0029 ')' */
static const uint8_t glyph_u0029_bitmap[] = {
    0x49, 0x12, 0x49, 0x25, 0x24, 
};

/* U+002A '*' */
static const uint8_t glyph_u002A_bitmap[] = {
    0x27, 0x9D, 0x62, 0x00, 
};

/* U+002B '+' */
static const uint8_t glyph_u002B_bitmap[] = {
    0x10, 0x20, 0x43, 0xE1, 0x02, 0x00, 
};

/* U+002C ',' */
static const uint8_t glyph_u002C_bitmap[] = {
    0x49, 0x20, 
};

/* U+002D '-' */
static const uint8_t glyph_u002D_bitmap[] = {
    0x70, 
};

/* U+002E '.' */
static const uint8_t glyph_u002E_bitmap[] = {
    0x48, 
};

/* U+002F '/' */
static const uint8_t glyph_u002F_bitmap[] = {
    0x00, 0x20, 0x82, 0x10, 0x41, 0x08, 0x20, 0x84, 0x10, 0x40, 
};

/* U+0030 '0' */
static const uint8_t glyph_u0030_bitmap[] = {
    0x3C, 0x66, 0x42, 0x42, 0x42, 0x42, 0x42, 0x66, 0x3C, 
};

/* U+0031 '1' */
static const uint8_t glyph_u0031_bitmap[] = {
    0xE2, 0x22, 0x22, 0x22, 0x20, 
};

/* U+0032 '2' */
static const uint8_t glyph_u0032_bitmap[] = {
    0x79, 0x98, 0x10, 0x20, 0x82, 0x0C, 0x30, 0xFC, 
};

/* U+0033 '3' */
static const uint8_t glyph_u0033_bitmap[] = {
    0xFC, 0x18, 0x60, 0x81, 0xC0, 0x81, 0x66, 0x78, 
};

/* U+0034 '4' */
static const uint8_t glyph_u0034_bitmap[] = {
    0x0C, 0x08, 0x10, 0x30, 0x24, 0x44, 0xFF, 0x04, 0x04, 
};

/* U+0035 '5' */
static const uint8_t glyph_u0035_bitmap[] = {
    0x7C, 0x81, 0x03, 0xC0, 0xC0, 0x81, 0x66, 0x78, 
};

/* U+0036 '6' */
static const uint8_t glyph_u0036_bitmap[] = {
    0x1C, 0x60, 0x40, 0x5C, 0x66, 0x42, 0x42, 0x66, 0x3C, 
};

/* U+0037 '7' */
static const uint8_t glyph_u0037_bitmap[] = {
    0xFF, 0x88, 0x10, 0x60, 0x81, 0x04, 0x08, 0x30, 
};

/* U+0038 '8' */
static const uint8_t glyph_u0038_bitmap[] = {
    0x3C, 0x66, 0x42, 0x46, 0x3C, 0x46, 0x42, 0x46, 0x3C, 
};

/* U+0039 '9' */
static const uint8_t glyph_u0039_bitmap[] = {
    0x38, 0x8B, 0x1A, 0x37, 0xE0, 0xC1, 0x06, 0x78, 
};

/* U+003A ':' */
static const uint8_t glyph_u003A_bitmap[] = {
    0x48, 0x00, 0x90, 
};

/* U+003B ';' */
static const uint8_t glyph_u003B_bitmap[] = {
    0x48, 0x00, 0x92, 0x40, 
};

/* U+003C '<' */
static const uint8_t glyph_u003C_bitmap[] = {
    0x00, 0x18, 0xC3, 0x03, 0x01, 0x80, 0x00, 
};

/* U+003D '=' */
static const uint8_t glyph_u003D_bitmap[] = {
    0x7C, 0x00, 0x03, 0xE0, 0x00, 
};

/* U+003E '>' */
static const uint8_t glyph_u003E_bitmap[] = {
    0x00, 0xC0, 0x60, 0x61, 0x8C, 0x00, 0x00, 
};

/* U+003F '?' */
static const uint8_t glyph_u003F_bitmap[] = {
    0x79, 0x98, 0x10, 0x61, 0x82, 0x00, 0x08, 0x10, 
};

/* U+0040 '@' */
static const uint8_t glyph_u0040_bitmap[] = {
    0x0F, 0x83, 0x04, 0x2F, 0xA4, 0x98, 0x50, 0x95, 0x09, 0x50, 0x94, 0x98, 
    0x4F, 0x62, 0x00, 0x30, 0x00, 0xF0, 
};

/* U+0041 'A' */
static const uint8_t glyph_u0041_bitmap[] = {
    0x04, 0x03, 0x80, 0xA0, 0x68, 0x11, 0x0C, 0x43, 0xF8, 0x82, 0x40, 0xC0, 
};

/* U+0042 'B' */
static const uint8_t glyph_u0042_bitmap[] = {
    0xFC, 0x86, 0x82, 0x86, 0xFC, 0xC2, 0x82, 0x82, 0xFC, 
};

/* U+0043 'C' */
static const uint8_t glyph_u0043_bitmap[] = {
    0x1E, 0x19, 0x90, 0x08, 0x04, 0x02, 0x01, 0x00, 0x66, 0x1E, 0x00, 
};

/* U+0044 'D' */
static const uint8_t glyph_u0044_bitmap[] = {
    0xFC, 0x63, 0x20, 0x50, 0x28, 0x14, 0x0A, 0x05, 0x8C, 0xFC, 0x00, 
};

/* U+0045 'E' */
static const uint8_t glyph_u0045_bitmap[] = {
    0xFD, 0x82, 0x04, 0x0F, 0xD8, 0x20, 0x60, 0xFC, 
};

/* U+0046 'F' */
static const uint8_t glyph_u0046_bitmap[] = {
    0xFD, 0x82, 0x04, 0x0F, 0xD8, 0x20, 0x40, 0x80, 
};

/* U+0047 'G' */
static const uint8_t glyph_u0047_bitmap[] = {
    0x1E, 0x19, 0x90, 0x08, 0x04, 0x12, 0x09, 0x04, 0x62, 0x1E, 0x00, 
};

/* U+0048 'H' */
static const uint8_t glyph_u0048_bitmap[] = {
    0x82, 0x82, 0x82, 0x82, 0xFE, 0xC2, 0x82, 0x82, 0x82, 
};

/* U+0049 'I' */
static const uint8_t glyph_u0049_bitmap[] = {
    0xAA, 0xAA, 0x80, 
};

/* U+004A 'J' */
static const uint8_t glyph_u004A_bitmap[] = {
    0x3C, 0x10, 0x41, 0x04, 0x10, 0x53, 0x38, 
};

/* U+004B 'K' */
static const uint8_t glyph_u004B_bitmap[] = {
    0x82, 0x84, 0x88, 0x90, 0xF0, 0xF8, 0xCC, 0x84, 0x82, 
};

/* U+004C 'L' */
static const uint8_t glyph_u004C_bitmap[] = {
    0x81, 0x02, 0x04, 0x08, 0x10, 0x20, 0x60, 0xFC, 
};

/* U+004D 'M' */
static const uint8_t glyph_u004D_bitmap[] = {
    0xC0, 0xB0, 0x6E, 0x1A, 0x8A, 0x92, 0xA5, 0x28, 0xCA, 0x22, 0x80, 0x80, 
};

/* U+004E 'N' */
static const uint8_t glyph_u004E_bitmap[] = {
    0xC2, 0xC2, 0xE2, 0xB2, 0x9A, 0x8A, 0x86, 0x86, 0x82, 
};

/* U+004F 'O' */
static const uint8_t glyph_u004F_bitmap[] = {
    0x1E, 0x0C, 0xC4, 0x09, 0x02, 0x40, 0x90, 0x24, 0x08, 0xCC, 0x1E, 0x00, 
};

/* U+0050 'P' */
static const uint8_t glyph_u0050_bitmap[] = {
    0xFC, 0xC6, 0x82, 0x82, 0x86, 0xFC, 0xC0, 0x80, 0x80, 
};

/* U+0051 'Q' */
static const uint8_t glyph_u0051_bitmap[] = {
    0x1E, 0x0C, 0xC4, 0x09, 0x02, 0x40, 0x90, 0x24, 0x08, 0xC4, 0x1E, 0x01, 
    0x90, 0x38, 0x00, 
};

/* U+0052 'R' */
static const uint8_t glyph_u0052_bitmap[] = {
    0xFC, 0xC6, 0x82, 0x82, 0x86, 0xFC, 0xCC, 0x84, 0x82, 
};

/* U+0053 'S' */
static const uint8_t glyph_u0053_bitmap[] = {
    0x3C, 0x81, 0x03, 0x03, 0x80, 0xC0, 0xA3, 0x7C, 
};

/* U+0054 'T' */
static const uint8_t glyph_u0054_bitmap[] = {
    0xFE, 0x20, 0x40, 0x81, 0x02, 0x04, 0x08, 0x10, 
};

/* U+0055 'U' */
static const uint8_t glyph_u0055_bitmap[] = {
    0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0xC2, 0x66, 0x3C, 
};

/* U+0056 'V' */
static const uint8_t glyph_u0056_bitmap[] = {
    0x40, 0x88, 0x22, 0x18, 0xC4, 0x11, 0x06, 0x80, 0xA0, 0x30, 0x0C, 0x00, 
};

/* U+0057 'W' */
static const uint8_t glyph_u0057_bitmap[] = {
    0x43, 0x09, 0x0C, 0x24, 0x71, 0x19, 0x44, 0x24, 0x90, 0xB2, 0x83, 0x8A, 
    0x06, 0x18, 0x18, 0x40, 
};

/* U+0058 'X' */
static const uint8_t glyph_u0058_bitmap[] = {
    0x42, 0x66, 0x24, 0x18, 0x18, 0x1C, 0x24, 0x62, 0xC3, 
};

/* U+0059 'Y' */
static const uint8_t glyph_u0059_bitmap[] = {
    0x61, 0x90, 0x8C, 0x82, 0x40, 0xC0, 0x60, 0x20, 0x10, 0x08, 0x00, 
};

/* U+005A 'Z' */
static const uint8_t glyph_u005A_bitmap[] = {
    0x7E, 0x06, 0x04, 0x08, 0x18, 0x30, 0x20, 0x60, 0x7F, 
};

/* U+005B '[' */
static const uint8_t glyph_u005B_bitmap[] = {
    0xF2, 0x49, 0x24, 0x92, 0x4E, 
};

/* U+005C '\' */
static const uint8_t glyph_u005C_bitmap[] = {
    0x01, 0x04, 0x18, 0x20, 0x82, 0x04, 0x10, 0x40, 0x82, 0x08, 
};

/* U+005D ']' */
static const uint8_t glyph_u005D_bitmap[] = {
    0xE4, 0x92, 0x49, 0x24, 0x9E, 
};

/* U+005E '^' */
static const uint8_t glyph_u005E_bitmap[] = {
    0x00, 0x20, 0xA1, 0x44, 0x48, 0x80, 
};

/* U+005F '_' */
static const uint8_t glyph_u005F_bitmap[] = {
    0xFC, 
};

/* U+0060 '`' */
static const uint8_t glyph_u0060_bitmap[] = {
    0x02, 
};

/* U+0061 'a' */
static const uint8_t glyph_u0061_bitmap[] = {
    0x78, 0x98, 0x13, 0xE4, 0x48, 0x9F, 0x00, 
};

/* U+0062 'b' */
static const uint8_t glyph_u0062_bitmap[] = {
    0x81, 0x02, 0x05, 0xCC, 0x50, 0xE1, 0xC3, 0xCD, 0x70, 
};

/* U+0063 'c' */
static const uint8_t glyph_u0063_bitmap[] = {
    0x3C, 0xC9, 0x02, 0x04, 0x0C, 0x8F, 0x00, 
};

/* U+0064 'd' */
static const uint8_t glyph_u0064_bitmap[] = {
    0x02, 0x02, 0x02, 0x3A, 0x66, 0x42, 0x42, 0x42, 0x66, 0x3A, 
};

/* U+0065 'e' */
static const uint8_t glyph_u0065_bitmap[] = {
    0x38, 0xC9, 0x0B, 0xF4, 0x0C, 0x8F, 0x00, 
};

/* U+0066 'f' */
static const uint8_t glyph_u0066_bitmap[] = {
    0x33, 0x11, 0xE4, 0x21, 0x08, 0x42, 0x00, 
};

/* U+0067 'g' */
static const uint8_t glyph_u0067_bitmap[] = {
    0x3A, 0x66, 0x42, 0x42, 0x42, 0x66, 0x3A, 0x02, 0x46, 0x3C, 
};

/* U+0068 'h' */
static const uint8_t glyph_u0068_bitmap[] = {
    0x81, 0x02, 0x05, 0xCC, 0xD0, 0xA1, 0x42, 0x85, 0x08, 
};

/* U+0069 'i' */
static const uint8_t glyph_u0069_bitmap[] = {
    0x48, 0x24, 0x92, 0x48, 
};

/* U+006A 'j' */
static const uint8_t glyph_u006A_bitmap[] = {
    0x10, 0x00, 0x21, 0x08, 0x42, 0x10, 0x84, 0x27, 0x00, 
};

/* U+006B 'k' */
static const uint8_t glyph_u006B_bitmap[] = {
    0x81, 0x02, 0x04, 0x69, 0x96, 0x3C, 0x6C, 0x89, 0x08, 
};

/* U+006C 'l' */
static const uint8_t glyph_u006C_bitmap[] = {
    0xAA, 0xAA, 0xA0, 
};

/* U+006D 'm' */
static const uint8_t glyph_u006D_bitmap[] = {
    0xBB, 0xD9, 0xCA, 0x11, 0xC2, 0x38, 0x47, 0x08, 0xE1, 0x18, 
};

/* U+006E 'n' */
static const uint8_t glyph_u006E_bitmap[] = {
    0xB9, 0x9A, 0x14, 0x28, 0x50, 0xA1, 0x00, 
};

/* U+006F 'o' */
static const uint8_t glyph_u006F_bitmap[] = {
    0x3C, 0x66, 0x42, 0x42, 0x42, 0x66, 0x3C, 
};

/* U+0070 'p' */
static const uint8_t glyph_u0070_bitmap[] = {
    0xB9, 0x8A, 0x1C, 0x38, 0x79, 0xAE, 0x40, 0x81, 0x00, 
};

/* U+0071 'q' */
static const uint8_t glyph_u0071_bitmap[] = {
    0x3A, 0x66, 0x42, 0x42, 0x42, 0x66, 0x3A, 0x02, 0x02, 0x02, 
};

/* U+0072 'r' */
static const uint8_t glyph_u0072_bitmap[] = {
    0xBC, 0x88, 0x88, 0x80, 
};

/* U+0073 's' */
static const uint8_t glyph_u0073_bitmap[] = {
    0x79, 0x04, 0x1E, 0x0C, 0x37, 0x80, 
};

/* U+0074 't' */
static const uint8_t glyph_u0074_bitmap[] = {
    0x02, 0x3C, 0x84, 0x21, 0x0C, 0x30, 
};

/* U+0075 'u' */
static const uint8_t glyph_u0075_bitmap[] = {
    0x85, 0x0A, 0x14, 0x28, 0x59, 0x9D, 0x00, 
};

/* U+0076 'v' */
static const uint8_t glyph_u0076_bitmap[] = {
    0x43, 0x22, 0x22, 0x34, 0x14, 0x1C, 0x08, 
};

/* U+0077 'w' */
static const uint8_t glyph_u0077_bitmap[] = {
    0x84, 0x29, 0x89, 0x29, 0x25, 0x63, 0x28, 0x63, 0x0C, 0x60, 
};

/* U+0078 'x' */
static const uint8_t glyph_u0078_bitmap[] = {
    0x44, 0xD0, 0xE1, 0x83, 0x89, 0x11, 0x00, 
};

/* U+0079 'y' */
static const uint8_t glyph_u0079_bitmap[] = {
    0x43, 0x22, 0x22, 0x24, 0x14, 0x1C, 0x18, 0x08, 0x10, 0x70, 
};

/* U+007A 'z' */
static const uint8_t glyph_u007A_bitmap[] = {
    0x7C, 0x21, 0x0C, 0x21, 0x0F, 0xC0, 
};

/* U+007B '{' */
static const uint8_t glyph_u007B_bitmap[] = {
    0x32, 0x22, 0x22, 0x46, 0x22, 0x22, 0x30, 
};

/* U+007C '|' */
static const uint8_t glyph_u007C_bitmap[] = {
    0xAA, 0xAA, 0xAA, 0x80, 
};

/* U+007D '}' */
static const uint8_t glyph_u007D_bitmap[] = {
    0xC4, 0x66, 0x66, 0x26, 0x66, 0x66, 0xC0, 
};

/* U+007E '~' */
static const uint8_t glyph_u007E_bitmap[] = {
    0x74, 0xB8, 
};

/* U+00B0 '?' */
static const uint8_t glyph_degree_bitmap[] = {
    0x22, 0x80, 0xA2, 0x00, 
};

/* U+2022 '?' */
static const uint8_t glyph_bullet_bitmap[] = {
    0x0D, 0x80, 
};

/* Glyph table */
static const ui_glyph_t montserrat_12_glyphs[] = {
    {32, 0, 0, 0, 0, 3, NULL},
    {33, 3, 9, 0, -9, 3, glyph_u0021_bitmap},
    {34, 4, 4, 0, -9, 5, glyph_u0022_bitmap},
    {35, 9, 9, 0, -9, 8, glyph_u0023_bitmap},
    {36, 7, 13, 0, -11, 7, glyph_u0024_bitmap},
    {37, 10, 9, 0, -9, 10, glyph_u0025_bitmap},
    {38, 9, 10, 0, -9, 8, glyph_u0026_bitmap},
    {39, 2, 4, 0, -9, 3, glyph_u0027_bitmap},
    {40, 3, 13, 1, -10, 4, glyph_u0028_bitmap},
    {41, 3, 13, 0, -10, 4, glyph_u0029_bitmap},
    {42, 5, 5, 0, -10, 5, glyph_u002A_bitmap},
    {43, 7, 6, 0, -8, 7, glyph_u002B_bitmap},
    {44, 3, 4, 0, -2, 3, glyph_u002C_bitmap},
    {45, 4, 2, 0, -4, 5, glyph_u002D_bitmap},
    {46, 3, 2, 0, -2, 3, glyph_u002E_bitmap},
    {47, 6, 13, -1, -12, 4, glyph_u002F_bitmap},
    {48, 8, 9, 0, -9, 8, glyph_u0030_bitmap},
    {49, 4, 9, 0, -9, 4, glyph_u0031_bitmap},
    {50, 7, 9, 0, -9, 7, glyph_u0032_bitmap},
    {51, 7, 9, 0, -9, 7, glyph_u0033_bitmap},
    {52, 8, 9, 0, -9, 8, glyph_u0034_bitmap},
    {53, 7, 9, 0, -9, 7, glyph_u0035_bitmap},
    {54, 8, 9, 0, -9, 7, glyph_u0036_bitmap},
    {55, 7, 9, 0, -9, 7, glyph_u0037_bitmap},
    {56, 8, 9, 0, -9, 8, glyph_u0038_bitmap},
    {57, 7, 9, 0, -9, 7, glyph_u0039_bitmap},
    {58, 3, 7, 0, -7, 3, glyph_u003A_bitmap},
    {59, 3, 9, 0, -7, 3, glyph_u003B_bitmap},
    {60, 7, 7, 0, -8, 7, glyph_u003C_bitmap},
    {61, 7, 5, 0, -7, 7, glyph_u003D_bitmap},
    {62, 7, 7, 0, -8, 7, glyph_u003E_bitmap},
    {63, 7, 9, 0, -9, 7, glyph_u003F_bitmap},
    {64, 12, 12, 0, -9, 12, glyph_u0040_bitmap},
    {65, 10, 9, -1, -9, 9, glyph_u0041_bitmap},
    {66, 8, 9, 1, -9, 9, glyph_u0042_bitmap},
    {67, 9, 9, 0, -9, 9, glyph_u0043_bitmap},
    {68, 9, 9, 1, -9, 10, glyph_u0044_bitmap},
    {69, 7, 9, 1, -9, 8, glyph_u0045_bitmap},
    {70, 7, 9, 1, -9, 8, glyph_u0046_bitmap},
    {71, 9, 9, 0, -9, 9, glyph_u0047_bitmap},
    {72, 8, 9, 1, -9, 10, glyph_u0048_bitmap},
    {73, 2, 9, 1, -9, 4, glyph_u0049_bitmap},
    {74, 6, 9, -1, -9, 6, glyph_u004A_bitmap},
    {75, 8, 9, 1, -9, 9, glyph_u004B_bitmap},
    {76, 7, 9, 1, -9, 7, glyph_u004C_bitmap},
    {77, 10, 9, 1, -9, 11, glyph_u004D_bitmap},
    {78, 8, 9, 1, -9, 10, glyph_u004E_bitmap},
    {79, 10, 9, 0, -9, 10, glyph_u004F_bitmap},
    {80, 8, 9, 1, -9, 9, glyph_u0050_bitmap},
    {81, 10, 12, 0, -9, 10, glyph_u0051_bitmap},
    {82, 8, 9, 1, -9, 9, glyph_u0052_bitmap},
    {83, 7, 9, 0, -9, 7, glyph_u0053_bitmap},
    {84, 7, 9, 0, -9, 7, glyph_u0054_bitmap},
    {85, 8, 9, 1, -9, 10, glyph_u0055_bitmap},
    {86, 10, 9, -1, -9, 9, glyph_u0056_bitmap},
    {87, 14, 9, 0, -9, 14, glyph_u0057_bitmap},
    {88, 8, 9, 0, -9, 8, glyph_u0058_bitmap},
    {89, 9, 9, -1, -9, 8, glyph_u0059_bitmap},
    {90, 8, 9, 0, -9, 8, glyph_u005A_bitmap},
    {91, 3, 13, 1, -10, 4, glyph_u005B_bitmap},
    {92, 6, 13, -1, -12, 4, glyph_u005C_bitmap},
    {93, 3, 13, 0, -10, 4, glyph_u005D_bitmap},
    {94, 7, 6, 0, -8, 7, glyph_u005E_bitmap},
    {95, 6, 1, 0, 0, 6, glyph_u005F_bitmap},
    {96, 4, 2, 1, -10, 7, glyph_u0060_bitmap},
    {97, 7, 7, 0, -7, 7, glyph_u0061_bitmap},
    {98, 7, 10, 1, -10, 8, glyph_u0062_bitmap},
    {99, 7, 7, 0, -7, 7, glyph_u0063_bitmap},
    {100, 8, 10, 0, -10, 8, glyph_u0064_bitmap},
    {101, 7, 7, 0, -7, 7, glyph_u0065_bitmap},
    {102, 5, 10, 0, -10, 4, glyph_u0066_bitmap},
    {103, 8, 10, 0, -7, 8, glyph_u0067_bitmap},
    {104, 7, 10, 1, -10, 8, glyph_u0068_bitmap},
    {105, 3, 10, 0, -10, 3, glyph_u0069_bitmap},
    {106, 5, 13, -2, -10, 3, glyph_u006A_bitmap},
    {107, 7, 10, 1, -10, 7, glyph_u006B_bitmap},
    {108, 2, 10, 1, -10, 3, glyph_u006C_bitmap},
    {109, 11, 7, 1, -7, 13, glyph_u006D_bitmap},
    {110, 7, 7, 1, -7, 8, glyph_u006E_bitmap},
    {111, 8, 7, 0, -7, 8, glyph_u006F_bitmap},
    {112, 7, 10, 1, -7, 8, glyph_u0070_bitmap},
    {113, 8, 10, 0, -7, 8, glyph_u0071_bitmap},
    {114, 4, 7, 1, -7, 5, glyph_u0072_bitmap},
    {115, 6, 7, 0, -7, 6, glyph_u0073_bitmap},
    {116, 5, 9, 0, -9, 5, glyph_u0074_bitmap},
    {117, 7, 7, 1, -7, 8, glyph_u0075_bitmap},
    {118, 8, 7, -1, -7, 7, glyph_u0076_bitmap},
    {119, 11, 7, 0, -7, 11, glyph_u0077_bitmap},
    {120, 7, 7, 0, -7, 7, glyph_u0078_bitmap},
    {121, 8, 10, -1, -7, 7, glyph_u0079_bitmap},
    {122, 6, 7, 0, -7, 6, glyph_u007A_bitmap},
    {123, 4, 13, 0, -10, 4, glyph_u007B_bitmap},
    {124, 2, 13, 1, -10, 4, glyph_u007C_bitmap},
    {125, 4, 13, 0, -10, 4, glyph_u007D_bitmap},
    {126, 7, 2, 0, -6, 7, glyph_u007E_bitmap},
    {176, 5, 5, 0, -10, 5, glyph_degree_bitmap},
    {8226, 3, 3, 0, -5, 4, glyph_bullet_bitmap},
};

/* Font descriptor */
const ui_font_t font_montserrat_12 = {
    .glyphs = montserrat_12_glyphs,
    .glyph_count = 97,
    .height = 15,
    .baseline = 12,
    .bpp = 1,
};
