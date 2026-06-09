#ifndef __TTP229_H__
#define __TTP229_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

/* ======================== Pin Assignments ======================== */

/* TTP229-BSF: PB15-SDO, PB14-SCL (16-key mode, all 16 channels) */
#define TTP_SDO_PORT       GPIOB
#define TTP_SDO_PIN        GPIO_Pin_15
#define TTP_SCL_PORT       GPIOB
#define TTP_SCL_PIN        GPIO_Pin_14

/* ======================== Constants ======================== */

#define TTP_KEY_COUNT      16      /* Total touch keys */
#define TTP_BITS_BYTES     2       /* 16 bits = 2 bytes bitmap */

/*
 * Touch bitmap layout (1-based key IDs, bit = key_id - 1):
 *   Grid (Keys 1-4):
 *     bit0=Key1(TP9,左上)  bit1=Key2(TP11,右上)
 *     bit2=Key3(TP8,左下)  bit3=Key4(TP10,右下)
 *   Ring (Keys 5-16, clockwise 12→11 o'clock):
 *     bit4=Key5(TP13,12h)  bit5=Key6(TP3,1h)  bit6=Key7(TP2,2h)
 *     bit7=Key8(TP1,3h)    bit8=Key9(TP0,4h)   bit9=Key10(TP15,5h)
 *     bit10=Key11(TP14,6h) bit11=Key12(TP7,7h)  bit12=Key13(TP6,8h)
 *     bit13=Key14(TP5,9h)  bit14=Key15(TP4,10h) bit15=Key16(TP12,11h)
 */

/* ======================== API ======================== */

void     TTP229_Init(void);
void     TTP229_Calibrate(void);
uint16_t TTP229_Read(void);   /* returns 16-bit touch bitmap (1=touched) */
uint16_t TTP229_GetRaw(void); /* returns cached raw data */

#ifdef __cplusplus
}
#endif

#endif /* __TTP229_H__ */
