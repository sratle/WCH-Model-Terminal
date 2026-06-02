#include "key.h"
#include "hardware.h"
#include "Protocol/protocol.h"
#include "Display/display.h"
#include "CS43131/cs43131.h"

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
            if (key_states[i].press_tick == 0)
            {
                key_states[i].press_tick = key_tick;
            }
            else if ((key_tick - key_states[i].press_tick) >= (KEY_DEBOUNCE_MS / 10))
            {
                key_states[i].pressed = 1;
                key_states[i].long_pressed = 0;
                *key_id = key_ids[i];
                return KEY_EVT_PRESS;
            }
        }
        else if (pin_val && key_states[i].pressed && !key_states[i].long_pressed)
        {
            if ((key_tick - key_states[i].press_tick) >= (KEY_LONG_PRESS_MS / 10))
            {
                key_states[i].long_pressed = 1;
                *key_id = key_ids[i];
                return KEY_EVT_LONG_PRESS;
            }
        }
        else if (!pin_val && key_states[i].pressed)
        {
            key_states[i].pressed = 0;
            key_states[i].long_pressed = 0;
            key_states[i].press_tick = 0;
            *key_id = key_ids[i];
            return KEY_EVT_RELEASE;
        }
        else
        {
            key_states[i].press_tick = 0;
        }
    }

    return KEY_EVT_NONE;
}

void Key_PollAndPush(void)
{
    key_event_t evt;
    uint8_t key_id = 0;

    evt = Key_Scan(&key_id);
    if (evt != KEY_EVT_NONE)
    {
        Hardware_KeyQueue_Push(key_id, (uint8_t)evt);
    }
}

static uint8_t key_id_to_proto(uint8_t key_id)
{
    switch (key_id)
    {
        case KEY_PLUS:  return CORE_KEY_PLUS;
        case KEY_SUB:   return CORE_KEY_SUB;
        case KEY_ENTER: return CORE_KEY_ENTER;
        default:        return 0;
    }
}

static uint8_t key_evt_to_proto(uint8_t evt)
{
    switch (evt)
    {
        case KEY_EVT_PRESS:      return CORE_KEY_EVT_PRESS;
        case KEY_EVT_RELEASE:    return CORE_KEY_EVT_RELEASE;
        case KEY_EVT_LONG_PRESS: return CORE_KEY_EVT_LONG_PRESS;
        default:                 return CORE_KEY_EVT_RELEASE;
    }
}

void Key_ProcessEvents(void)
{
    core_key_event_t kevt;
    uint8_t report[2];

    while (Hardware_KeyQueue_Pop(&kevt))
    {
        report[0] = key_evt_to_proto(kevt.event);
        report[1] = key_id_to_proto(kevt.key_id);

        Display_SendInputEvent(&display_g, INPUT_DEV_CORE_KEY,
                               report, 2);

        if (kevt.event == KEY_EVT_PRESS || kevt.event == KEY_EVT_LONG_PRESS)
        {
            if (kevt.key_id == KEY_SUB)
            {
                printf("[KEY] SUB pressed\r\n");
                uint8_t vol = Audio_GetVolume();
                if (vol >= 5)
                    Audio_SetVolume(vol - 5);
                else
                    Audio_SetVolume(0);
            }
            else if (kevt.key_id == KEY_PLUS)
            {
                printf("[KEY] PLUS pressed\r\n");
                uint8_t vol = Audio_GetVolume();
                if (vol <= 95)
                    Audio_SetVolume(vol + 5);
                else
                    Audio_SetVolume(100);
            }
        }
    }
}
