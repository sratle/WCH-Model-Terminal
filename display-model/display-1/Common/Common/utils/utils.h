/********************************** (C) COPYRIGHT *******************************
* File Name          : utils.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Description        : Lightweight utility functions for games.
*                      Avoids stdio.h/stdlib.h to prevent newlib bloat.
********************************************************************************/
#ifndef __UTILS_H
#define __UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Lightweight string-to-int (positive numbers only)
 * @param  str: null-terminated string containing digits
 * @retval integer value, 0 if no digits found
 */
static inline int utils_atoi(const char *str)
{
    int val = 0;
    while (*str >= '0' && *str <= '9') {
        val = val * 10 + (*str - '0');
        str++;
    }
    return val;
}

/**
 * @brief  Lightweight snprintf for "appcfg set <game> highscore <value>" pattern
 * @param  buf: output buffer
 * @param  size: buffer size
 * @param  fmt: format string with exactly one %d
 * @param  val: integer value to insert
 * @retval number of characters written (excluding null terminator)
 */
static inline int utils_snprintf(char *buf, int size, const char *fmt, int val)
{
    const char *p = fmt;
    char *out = buf;
    int remain = size - 1; /* reserve 1 for '\0' */

    while (*p && remain > 0) {
        if (*p == '%' && *(p + 1) == 'd') {
            /* Convert int to string */
            char num[12];
            int n = val;
            int neg = 0;
            int i = 0;
            if (n < 0) {
                neg = 1;
                n = -n;
            }
            if (n == 0) {
                num[i++] = '0';
            } else {
                int j = 0;
                char tmp[12];
                while (n > 0) {
                    tmp[j++] = '0' + (n % 10);
                    n /= 10;
                }
                if (neg) num[i++] = '-';
                while (j > 0) num[i++] = tmp[--j];
            }
            num[i] = '\0';

            /* Copy number to output */
            int k = 0;
            while (num[k] && remain > 0) {
                *out++ = num[k++];
                remain--;
            }
            p += 2; /* skip %d */
        } else {
            *out++ = *p++;
            remain--;
        }
    }
    *out = '\0';
    return (int)(out - buf);
}

#ifdef __cplusplus
}
#endif

#endif /* __UTILS_H */
