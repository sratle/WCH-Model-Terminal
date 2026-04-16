#include "key.h"

/**
 * @brief  Key initialization
 * @param  None
 * @retval None
 * @note   Initialize the key.PF10-KEY_PLUS,PF9-KEY_SUB,PF8-KEY_ENTER
 */
void Key_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.GPIO_Pin = KEY_ALL_PINS;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(KEY_GPIO_PORT, &GPIO_InitStructure);

    GPIO_ResetBits(KEY_GPIO_PORT, KEY_ALL_PINS);
}

uint8_t Key_scan(void)
{
    uint8_t key = 0;

    if(GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY_PLUS_PIN) == Bit_RESET)
    {
        Delay_Ms(10);
        if(GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY_PLUS_PIN) == Bit_RESET)
        {
            key = KEY_PLUS; /* KEY_PLUS */
        }
    }
    else if(GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY_SUB_PIN) == Bit_RESET)
    {
        Delay_Ms(10);
        if(GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY_SUB_PIN) == Bit_RESET)
        {
            key = KEY_SUB; /* KEY_SUB */
        }
    }
    else if(GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY_ENTER_PIN) == Bit_RESET)
    {
        Delay_Ms(10);
        if(GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY_ENTER_PIN) == Bit_RESET)
        {
            key = KEY_ENTER; /* KEY_ENTER */
        }
    }

    return key;
}
