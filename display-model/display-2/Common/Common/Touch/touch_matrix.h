/********************************** (C) COPYRIGHT *******************************
* File Name          : touch_matrix.h
* Author             : E-ink Model Team
* Version            : V1.1.0
* Date               : 2026/07/05
* Description        : TTP229 dual-chip 4x8 touch matrix driver API.
*
* Two TTP229-BSF chips form a 4-column x 8-row touch matrix:
*   TTP229-1 (SCL1=PB8, SDO1=PB9) -> TP0..TP15
*   TTP229-2 (SCL2=PB10, SDO2=PB11) -> TP16..TP31
*
* Driver uses relative displacement (touchpad-like) mode and feeds events
* to the MiniUI input system via ui_input_inject_touch().
********************************************************************************/
#ifndef __TOUCH_MATRIX_H__
#define __TOUCH_MATRIX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "../MiniUI/miniui_types.h"

/*=============================================================================
 *  Pin Assignments
 *===========================================================================*/
#define TM_SCL1_BIT         (1U << 8)    /* PB8 */
#define TM_SDO1_BIT         (1U << 9)    /* PB9 */
#define TM_SCL2_BIT         (1U << 10)   /* PB10 */
#define TM_SDO2_BIT         (1U << 11)   /* PB11 */

/*=============================================================================
 *  Matrix Geometry
 *===========================================================================*/
#define TM_ROWS             8
#define TM_COLS             4
#define TM_PAD_COUNT        32

/*=============================================================================
 *  Sensitivity (pixels per grid unit)
 *===========================================================================*/
#define TM_SENSITIVITY_X    50
#define TM_SENSITIVITY_Y    40

/*=============================================================================
 *  Screen Dimensions
 *===========================================================================*/
#define TM_SCREEN_W         648
#define TM_SCREEN_H         480

/*=============================================================================
 *  Public API
 *===========================================================================*/

void     TouchMatrix_Init(void);
void     TouchMatrix_Scan(void);
void     TouchMatrix_Calibrate(void);
void     TouchMatrix_SetSensitivity(int16_t sx, int16_t sy);

int16_t  TouchMatrix_GetCursorX(void);
int16_t  TouchMatrix_GetCursorY(void);
bool     TouchMatrix_IsTouched(void);
bool     TouchMatrix_IsCursorVisible(void);
uint32_t TouchMatrix_GetRawState(void);

/* Cursor dirty region management (called by ui_system around page draw) */
void     TouchMatrix_InvalidateCursor(void);
void     TouchMatrix_CursorRendered(void);

/* Bounding box of old+new cursor positions (for flush coalescing) */
void     TouchMatrix_GetCursorBBox(ui_rect_t *r);

#ifdef __cplusplus
}
#endif

#endif /* __TOUCH_MATRIX_H__ */
