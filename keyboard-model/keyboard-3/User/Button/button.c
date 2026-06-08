#include "button.h"

/* ======================== Pin Table ======================== */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} button_pin_t;

static const button_pin_t btn_pins[BUTTON_COUNT] = {
    { GPIOB, GPIO_Pin_9  },  /* KEY1  */
    { GPIOB, GPIO_Pin_8  },  /* KEY2  */
    { GPIOB, GPIO_Pin_7  },  /* KEY3  */
    { GPIOB, GPIO_Pin_6  },  /* KEY4  */
    { GPIOA, GPIO_Pin_4  },  /* KEY5  */
    { GPIOA, GPIO_Pin_5  },  /* KEY6  */
    { GPIOA, GPIO_Pin_6  },  /* KEY7  */
    { GPIOA, GPIO_Pin_7  },  /* KEY8  */
    { GPIOB, GPIO_Pin_0  },  /* KEY9  */
    { GPIOB, GPIO_Pin_1  },  /* KEY10 */
    { GPIOB, GPIO_Pin_10 },  /* KEY11 */
    { GPIOB, GPIO_Pin_11 },  /* KEY12 */
};

/* ======================== State ======================== */

static uint8_t btn_raw[BUTTON_COUNT];       /* raw GPIO read (0/1) */
static uint8_t btn_debounce[BUTTON_COUNT];   /* debounce counter */
static uint8_t btn_state[BUTTON_COUNT];      /* confirmed state (0=released, 1=pressed) */
static uint8_t btn_bitmap[BUTTON_BITS_BYTES]; /* output bitmap */

/* ======================== Public API ======================== */

void Button_Init(void)
{
    GPIO_InitTypeDef gpio;
    uint8_t i;

    /* Enable GPIOA and GPIOB clocks */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    /* Configure all button pins as IPD (input pull-down) */
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode  = GPIO_Mode_IPD;

    /* GPIOA: PA4, PA5, PA6, PA7 */
    gpio.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_Init(GPIOA, &gpio);

    /* GPIOB: PB0, PB1, PB6, PB7, PB8, PB9, PB10, PB11 */
    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_6 | GPIO_Pin_7 |
                    GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_Init(GPIOB, &gpio);

    /* Clear state */
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        btn_raw[i]     = 0;
        btn_debounce[i] = 0;
        btn_state[i]   = 0;
    }
    btn_bitmap[0] = 0;
    btn_bitmap[1] = 0;
}

void Button_Scan(void)
{
    uint8_t i;
    uint8_t reading;

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        /* Read GPIO (active high: 1 = pressed) */
        reading = (btn_pins[i].port->INDR & btn_pins[i].pin) ? 1 : 0;

        if (reading != btn_raw[i])
        {
            /* Reading changed, reset debounce counter */
            btn_raw[i] = reading;
            btn_debounce[i] = 0;
        }
        else
        {
            /* Reading stable, increment counter */
            if (btn_debounce[i] < BUTTON_DEBOUNCE_CNT)
            {
                btn_debounce[i]++;
            }
            else if (btn_state[i] != reading)
            {
                /* Debounce threshold reached, update confirmed state */
                btn_state[i] = reading;

                /* Update bitmap */
                if (reading)
                {
                    /* Pressed: set bit */
                    btn_bitmap[i >> 3] |= (1u << (i & 7));
                }
                else
                {
                    /* Released: clear bit */
                    btn_bitmap[i >> 3] &= ~(1u << (i & 7));
                }
            }
        }
    }
}

void Button_GetBitmap(uint8_t bitmap[BUTTON_BITS_BYTES])
{
    bitmap[0] = btn_bitmap[0];
    bitmap[1] = btn_bitmap[1];
}
