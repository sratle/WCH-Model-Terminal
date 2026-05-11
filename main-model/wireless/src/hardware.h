/********************************** (C) COPYRIGHT *******************************
 * File Name          : hardware.h
 * Description        : Board init, GPIO, UART1 debug, task IDs
 *******************************************************************************/

#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#include "CH58x_common.h"

/* TMOS task IDs */
extern tmosTaskID g_spi_task_id;
extern tmosTaskID g_bt_task_id;

/* Events */
#define SPI_PROCESS_EVT         0x0001
#define BT_PROCESS_EVT          0x0001

void Hardware_Init(void);
void Hardware_GPIOInit(void);
void Hardware_UART1Init(void);
void Hardware_SPI0Init(void);

uint32_t Hardware_GetMillis(void);

#endif /* __HARDWARE_H__ */
