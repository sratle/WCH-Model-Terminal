#include "key.h"
#include "hardware.h"
#include "Protocol/protocol.h"
#include "Display/display.h"
#include "CS43131/cs43131.h"
#include "Config/config.h"

static key_state_t key_states[3];
static uint32_t key_tick = 0;

static const uint16_t key_pins[3] = {
    KEY_PLUS_PIN,
    KEY_SUB_PIN,
    KEY_ENTER_PIN
};

static const uint8_t key_ids[3] = {
    KEY_PLUS,
    KEY_SUB,
    KEY_ENTER
};

void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOF, ENABLE);

    GPIO_InitStructure.GPIO_Pin = KEY_ALL_PINS;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(KEY_GPIO_PORT, &GPIO_InitStructure);

    memset(key_states, 0, sizeof(key_states));
    key_tick = 0;

    printf("[KEY] Init done. CFGHR=0x%08X OUTDR=0x%04X INDR=0x%04X\r\n",
           GPIOF->CFGHR, GPIOF->OUTDR, GPIOF->INDR);
    printf("[KEY] PF8=%d PF9=%d PF10=%d\r\n",
           GPIO_ReadInputDataBit(GPIOF, GPIO_Pin_8),
           GPIO_ReadInputDataBit(GPIOF, GPIO_Pin_9),
           GPIO_ReadInputDataBit(GPIOF, GPIO_Pin_10));
}

key_event_t Key_Scan(uint8_t *key_id)
{
    uint8_t i;
    uint8_t pin_val;

    key_tick++;

    for (i = 0; i < 3; i++)
    {
        pin_val = (GPIO_ReadInputDataBit(KEY_GPIO_PORT, key_pins[i]) == Bit_RESET) ? 1 : 0;

        if (pin_val && !key_states[i].pressed)
        {
            /* 按键按下中，尚未确认 */
            if (key_states[i].press_tick == 0)
            {
                key_states[i].press_tick = key_tick;
            }
            else if ((key_tick - key_states[i].press_tick) >= (KEY_DEBOUNCE_MS / 10))
            {
                /* 去抖通过，确认按下 */
                key_states[i].pressed = 1;
                key_states[i].long_pressed = 0;
                *key_id = key_ids[i];
                return KEY_EVT_PRESS;
            }
        }
        else if (pin_val && key_states[i].pressed && !key_states[i].long_pressed)
        {
            /* 已按下，检测长按 */
            if ((key_tick - key_states[i].press_tick) >= (KEY_LONG_PRESS_MS / 10))
            {
                key_states[i].long_pressed = 1;
                *key_id = key_ids[i];
                return KEY_EVT_LONG_PRESS;
            }
        }
        else if (!pin_val && key_states[i].pressed)
        {
            /* 已确认的按键释放 */
            key_states[i].pressed = 0;
            key_states[i].long_pressed = 0;
            key_states[i].press_tick = 0;
            *key_id = key_ids[i];
            return KEY_EVT_RELEASE;
        }
        else if (!pin_val && !key_states[i].pressed)
        {
            /* 按键空闲，清除去抖计数器 */
            key_states[i].press_tick = 0;
        }
        /* pin_val=1 && pressed=1 && long_pressed=1: 长按持续中，不产生事件 */
    }

    return KEY_EVT_NONE;
}

void Key_PollAndProcess(void)
{
    key_event_t evt;
    uint8_t key_id = 0;

    evt = Key_Scan(&key_id);
    if (evt == KEY_EVT_NONE)
        return;

    /* 核心按键仅在本地处理，不再向 Display 转发事件 */

    /* PLUS / SUB：本地音量调节（按下与长按均生效） */
    if (evt == KEY_EVT_PRESS || evt == KEY_EVT_LONG_PRESS)
    {
        if (key_id == KEY_SUB)
        {
            printf("[KEY] SUB pressed\r\n");
            int step = 5;
            Config_GetInt("0000", "volume_step", &step);
            uint8_t vol = Audio_GetVolume();
            if (vol >= step)
                Audio_SetVolume(vol - step);
            else
                Audio_SetVolume(0);
        }
        else if (key_id == KEY_PLUS)
        {
            printf("[KEY] PLUS pressed\r\n");
            int step = 5;
            Config_GetInt("0000", "volume_step", &step);
            uint8_t vol = Audio_GetVolume();
            if (vol <= 100 - step)
                Audio_SetVolume(vol + step);
            else
                Audio_SetVolume(100);
        }
    }

    /* ENTER：切换是否外放（功放 SHUTDOWN），仅在按下沿触发一次 */
    if (evt == KEY_EVT_PRESS && key_id == KEY_ENTER)
    {
        if (Speaker_GetState() != 0)
        {
            Speaker_Off(SPEAKER_CHANNEL_BOTH);
            printf("[KEY] ENTER: speaker OFF\r\n");
        }
        else
        {
            Speaker_On(SPEAKER_CHANNEL_BOTH);
            printf("[KEY] ENTER: speaker ON\r\n");
        }
    }
}
