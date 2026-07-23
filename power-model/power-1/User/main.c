/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : Power Team
 * Version            : V2.0.0
 * Date               : 2026/07/22
 * Description        : Power-1 module firmware (CH32V103C8T6).
 *
 *   职责：
 *     1. 通过 USART1（230400/8-N-1）回复 Core 的 GET_TYPE 心跳；
 *     2. 解析 IP5568 驱动的 188 型（YF2252SR-5）查理复用数码管，
 *        还原真实电量数字 0~100 + 充电状态，变化时上报 Core。
 *
 *   ===== 硬件接线 =====
 *     数码管 PIN1..PIN5 = IP5568 SMG1..SMG5 = V103 PB3..PB7
 *       PIN1->PB3  PIN2->PB4  PIN3->PB5  PIN4->PB6  PIN5->PB7
 *     USART1 TX(PA9)/RX(PA10) <-> Core UART5
 *
 *   ===== 188 = 5 脚查理复用(Charlieplexing)数显 =====
 *   每个段是一颗 LED，接在某“高电平脚(anode)”与“低电平脚(cathode)”之间；
 *   IP5568 逐时隙把一个脚拉高、若干脚拉低、其余高阻(Hi-Z)，快速轮扫拼出数字。
 *   数据手册图3 的段码表（anode->cathode，脚号 1~5）：
 *     百位"1": B1=3->4  C1=2->4
 *     十位   : A2=2->3  B2=3->2  C2=4->3  D2=4->2  E2=5->2  F2=5->3  G2=5->4
 *     个位   : A3=1->2  B3=2->1  C3=1->3  D3=3->1  E3=1->4  F3=4->1  G3=5->1
 *
 *   ===== 普通 GPIO 旁路解码（双配置法）=====
 *   查理复用靠三态，普通 IO 只有高/低两态，无法直接分辨“驱动低”与“Hi-Z”。
 *   利用内部上/下拉在同一扫描时隙内做两次读取来分离：
 *     - 内部下拉(IPD)读: 被驱动高的脚=1，其余(Hi-Z/驱动低)=0  -> 得到 anode；
 *     - 内部上拉(IPU)读: 被驱动低的脚=0，其余(Hi-Z/anode)=1   -> 得到 cathode 集；
 *   两次读在同一时隙内（相隔仅数十 µs，远小于 ms 级扫描时隙）即可配对出
 *   (anode->cathode) => 点亮的段。多时隙累计后按阈值判定“点亮”，再解码 7 段->数字。
 *   若实际扫描过快或需外接上/下拉，调 CP_* 参数与接线即可。
 *
 *   ===== 充电状态（数据手册闪烁规律，与数字解码正交）=====
 *     0.5Hz 整屏闪烁 -> 充电中(未充满)；1Hz -> 放电<5%；常亮 -> 放电/已满。
 *******************************************************************************/

#include "debug.h"

/* ============================================================================
 * 协议常量
 * ============================================================================ */
#define PWR_FRAME_HEAD              0xAA
#define PWR_FRAME_TAIL0             0xA5
#define PWR_FRAME_TAIL1             0x5A
#define PWR_FRAME_TAIL2             0xFC
#define PWR_FRAME_TAIL3             0xFD

#define MODULE_ID_CORE              0x00
#define MODULE_ID_POWER             0x30

#define CMD_GET_TYPE                0x01
#define CMD_ACK                     0x04
#define CMD_PWR_STATUS_REPORT       0x36

#define MODULE_TYPE_POWER           0x03
#define MODULE_SUBTYPE_POWER_STANDARD 0x01

#define POWER_FW_MAJOR              0x02
#define POWER_FW_MINOR              0x00
#define POWER_HW_VER                0x01

/* ============================================================================
 * 数码管 GPIO：PB3..PB7 = 数码管脚 1..5
 * ============================================================================ */
#define SMG_PORT                    GPIOB
#define SMG_MASK                    0x00F8u          /* PB3..PB7 */
#define PIN_BIT(p)                  (1u << ((p) + 2))/* 脚号 1..5 -> PB bit */

/* CFGLR 中把 PB3..PB7 配成“输入带上下拉”(CNF=10,MODE=00 -> 0x8)，
 * 之后仅靠 OUTDR 位切换上拉(1)/下拉(0)，无需重配寄存器，切换极快 */
#define SMG_SET_PULLDOWN()          (SMG_PORT->OUTDR &= ~SMG_MASK)
#define SMG_SET_PULLUP()            (SMG_PORT->OUTDR |=  SMG_MASK)
#define SMG_READ()                  ((uint16_t)(SMG_PORT->INDR & SMG_MASK))

/* 解码采样参数（可按实测扫描速率调整） */
#define CP_SETTLE_US                6      /* 切换上/下拉后建立时间 */
#define CP_GAP_US                   20     /* 相邻采样间隔 */
#define CP_SAMPLES                  1000   /* 一轮重建的总采样次数 */
#define CP_BATCH                    100    /* 每主循环推进的采样批量(≈非阻塞) */
#define CP_LIT_MIN                  3      /* 段被判定点亮所需最少命中次数 */

/* 充电闪烁“整屏是否点亮”快速采样 */
#define ACTIVE_SAMPLES              8
#define ACTIVE_GAP_US               250

/* 主循环节拍与判定（非阻塞：单圈仅 ~LOOP_MS + 小批采样） */
#define LOOP_MS                     20
#define EVAL_MS                     1000
#define BLINK_STEADY_MS             2500
#define BLINK_SLOW_MIN              700
#define BLINK_SLOW_MAX              1500
#define BLINK_FAST_MIN              250
#define BLINK_FAST_MAX              700

/* ============================================================================
 * 查理复用段码表
 * ============================================================================ */
enum {
    SEG_B1, SEG_C1,
    SEG_A2, SEG_B2, SEG_C2, SEG_D2, SEG_E2, SEG_F2, SEG_G2,
    SEG_A3, SEG_B3, SEG_C3, SEG_D3, SEG_E3, SEG_F3, SEG_G3,
    SEG_COUNT
};

typedef struct { uint8_t anode; uint8_t cathode; } cp_seg_t;

/* 索引与上面 enum 一一对应（anode->cathode，脚号 1..5） */
static const cp_seg_t s_cp_map[SEG_COUNT] = {
    {3, 4}, {2, 4},                                        /* B1 C1 */
    {2, 3}, {3, 2}, {4, 3}, {4, 2}, {5, 2}, {5, 3}, {5, 4},/* A2..G2 */
    {1, 2}, {2, 1}, {1, 3}, {3, 1}, {1, 4}, {4, 1}, {5, 1} /* A3..G3 */
};

/* 显示状态判定结果 */
enum {
    DISP_STEADY_OFF = 0, DISP_STEADY_ON, DISP_BLINK_SLOW, DISP_BLINK_FAST
};

/* ============================================================================
 * 接收状态机
 * ============================================================================ */
enum {
    RX_WAIT_HEAD = 0, RX_WAIT_SRC, RX_WAIT_DST, RX_WAIT_LEN, RX_WAIT_CMD,
    RX_WAIT_DATA, RX_WAIT_T0, RX_WAIT_T1, RX_WAIT_T2, RX_WAIT_T3
};

/* 最近一帧命令/目标（仅 ISR 内使用） */
static volatile uint8_t  g_rx_cmd;
static volatile uint8_t  g_rx_dst;

/* 闪烁检测 & 当前电量/充电（ISR 心跳应答时携带当前值） */
static uint32_t s_ms = 0;
static uint32_t s_last_eval_ms = 0;
static uint8_t  s_disp_lit = 0;
static uint32_t s_last_edge_ms = 0;
static uint32_t s_last_interval = 0;

static uint8_t  s_reported_pct    = 0;   /* 当前电量 0~100 */
static uint8_t  s_reported_charge = 0;   /* 当前充电态 0/1 */
static uint8_t  s_have_batt       = 0;   /* 是否已有有效电量 */

/* 分块重建状态（非阻塞，多圈累计） */
static uint16_t s_cp_cnt[SEG_COUNT];
static uint16_t s_cp_done;
static uint8_t  s_cp_running;
static uint8_t  s_number_ready;
static int16_t  s_pending_number;

/* ============================================================================
 * UART
 * ============================================================================ */
static void Power_UART_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};
    NVIC_InitTypeDef  NVIC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 230400;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel            = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd         = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

static void Power_UART_Send(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
            ;
        USART_SendData(USART1, data[i]);
    }
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
        ;
}

static void Power_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
    uint8_t buf[16];
    uint8_t n = 0;
    uint8_t i;

    if (data_len > sizeof(buf) - 9) data_len = sizeof(buf) - 9;

    buf[n++] = PWR_FRAME_HEAD;
    buf[n++] = MODULE_ID_POWER;
    buf[n++] = MODULE_ID_CORE;
    buf[n++] = (uint8_t)(1 + data_len);
    buf[n++] = cmd;
    for (i = 0; i < data_len; i++)
        buf[n++] = data[i];
    buf[n++] = PWR_FRAME_TAIL0;
    buf[n++] = PWR_FRAME_TAIL1;
    buf[n++] = PWR_FRAME_TAIL2;
    buf[n++] = PWR_FRAME_TAIL3;

    Power_UART_Send(buf, n);
}

static void Power_SendGetTypeAck(void)
{
    uint8_t d[5];
    d[0] = MODULE_TYPE_POWER;
    d[1] = MODULE_SUBTYPE_POWER_STANDARD;
    d[2] = POWER_HW_VER;
    d[3] = POWER_FW_MAJOR;
    d[4] = POWER_FW_MINOR;
    Power_SendFrame(CMD_ACK, d, 5);
}

/* DATA = [电量:1][充电:1] */
static void Power_SendStatus(uint8_t pct, uint8_t charging)
{
    uint8_t d[2];
    d[0] = pct;
    d[1] = charging ? 1 : 0;
    Power_SendFrame(CMD_PWR_STATUS_REPORT, d, 2);
}

static uint8_t Power_RxParse(uint8_t b)
{
    static uint8_t st = RX_WAIT_HEAD;
    static uint8_t f_dst, f_len, f_cmd;
    static uint8_t dcnt;
    uint8_t done = 0;

    switch (st) {
    case RX_WAIT_HEAD: if (b == PWR_FRAME_HEAD) st = RX_WAIT_SRC; break;
    case RX_WAIT_SRC:  st = RX_WAIT_DST; break;
    case RX_WAIT_DST:  f_dst = b; st = RX_WAIT_LEN; break;
    case RX_WAIT_LEN:
        f_len = b;
        st = (f_len == 0) ? RX_WAIT_HEAD : RX_WAIT_CMD;
        break;
    case RX_WAIT_CMD:
        f_cmd = b; dcnt = 0;
        st = (f_len == 1) ? RX_WAIT_T0 : RX_WAIT_DATA;
        break;
    case RX_WAIT_DATA:
        dcnt++;
        if (dcnt >= (uint8_t)(f_len - 1)) st = RX_WAIT_T0;
        break;
    case RX_WAIT_T0: st = (b == PWR_FRAME_TAIL0) ? RX_WAIT_T1 : RX_WAIT_HEAD; break;
    case RX_WAIT_T1: st = (b == PWR_FRAME_TAIL1) ? RX_WAIT_T2 : RX_WAIT_HEAD; break;
    case RX_WAIT_T2: st = (b == PWR_FRAME_TAIL2) ? RX_WAIT_T3 : RX_WAIT_HEAD; break;
    case RX_WAIT_T3:
        if (b == PWR_FRAME_TAIL3) {
            g_rx_cmd = f_cmd; g_rx_dst = f_dst; done = 1;
        }
        st = RX_WAIT_HEAD;
        break;
    default: st = RX_WAIT_HEAD; break;
    }
    return done;
}

void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t b = (uint8_t)USART_ReceiveData(USART1);
        /* 完整帧到达即处理：发给本模块的 GET_TYPE 立即应答（<1ms），
         * 不受主循环数码管采样阻塞影响。ISR 是唯一 TX 点，避免与主循环抢 UART */
        if (Power_RxParse(b) &&
            g_rx_dst == MODULE_ID_POWER && g_rx_cmd == CMD_GET_TYPE) {
            Power_SendGetTypeAck();
            if (s_have_batt)
                Power_SendStatus(s_reported_pct, s_reported_charge);
        }
    }
}

/* ============================================================================
 * 数码管解码
 * ============================================================================ */
static void Power_SMG_Init(void)
{
    uint32_t cfg;
    uint8_t p;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* 释放 PB3(JTDO)/PB4(NJTRST)：CH32V103 仅整体 SWJ 关闭，会同时关调试口 */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);

    /* PB3..PB7 = 输入带上下拉 (CNF=10, MODE=00 => nibble 0x8) */
    cfg = SMG_PORT->CFGLR;
    for (p = 3; p <= 7; p++) {
        cfg &= ~(0xFu << (4 * p));
        cfg |=  (0x8u << (4 * p));
    }
    SMG_PORT->CFGLR = cfg;
    SMG_SET_PULLDOWN();
}

/* 单个 5bit 值中若恰有一位置 1，返回其脚号(1..5)，否则 0 */
static uint8_t cp_single_pin(uint16_t bits)
{
    uint8_t p;
    if (bits == 0 || (bits & (uint16_t)(bits - 1)) != 0)
        return 0;                       /* 0 个或多个 -> 无效 */
    for (p = 1; p <= 5; p++)
        if (bits & PIN_BIT(p)) return p;
    return 0;
}

/* 仅凭 cathode 集 + 段码表反推 anode（IPD 读因 LED 耦合而模糊时的兜底）。
 * IPU 读得到的“驱动低”脚(cathode)较可靠：真正被拉低为干净 0V，
 * 而 Hi-Z 经 LED 耦合的中间电平被上拉判为 1。据此在段码表中找唯一
 * 满足“对集合内每个 cathode 都存在段 (A->c)”的 anode；唯一才返回，否则 0。 */
static uint8_t cp_infer_anode(uint16_t lo)
{
    uint8_t A, c, s;
    uint8_t found = 0, res = 0;

    if (lo == 0) return 0;
    for (A = 1; A <= 5; A++) {
        uint8_t all_ok, any, hit;
        if (lo & PIN_BIT(A)) continue;          /* anode 不会同时被驱动低 */
        all_ok = 1; any = 0;
        for (c = 1; c <= 5; c++) {
            if (!(lo & PIN_BIT(c))) continue;
            any = 1; hit = 0;
            for (s = 0; s < SEG_COUNT; s++)
                if (s_cp_map[s].anode == A && s_cp_map[s].cathode == c) { hit = 1; break; }
            if (!hit) { all_ok = 0; break; }
        }
        if (any && all_ok) { found++; res = A; }
    }
    return (found == 1) ? res : 0;
}

/* 7 段位型 -> 数字(0-9)，全灭返回 -1(空位)，无法识别返回 -2 */
static int8_t cp_decode_digit(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                              uint8_t e, uint8_t f, uint8_t g)
{
    uint8_t p = (uint8_t)((a << 6) | (b << 5) | (c << 4) | (d << 3) |
                          (e << 2) | (f << 1) | g);
    switch (p) {
    case 0x00: return -1;   /* 空位 */
    case 0x7E: return 0;
    case 0x30: return 1;
    case 0x6D: return 2;
    case 0x79: return 3;
    case 0x33: return 4;
    case 0x5B: return 5;
    case 0x5F: return 6;
    case 0x70: return 7;
    case 0x7F: return 8;
    case 0x7B: return 9;
    default:   return -2;   /* 未识别 */
    }
}

/* 启动新一轮分块重建：清零累计器 */
static void Power_Charlie_Begin(void)
{
    uint8_t s;
    for (s = 0; s < SEG_COUNT; s++) s_cp_cnt[s] = 0;
    s_cp_done = 0;
    s_cp_running = 1;
}

/* 每主循环推进一小批双配置采样（非阻塞）；完成整轮返回 1 */
static uint8_t Power_Charlie_Step(void)
{
    uint16_t i;
    uint8_t  s;

    if (!s_cp_running) return 0;

    for (i = 0; i < CP_BATCH && s_cp_done < CP_SAMPLES; i++, s_cp_done++) {
        uint16_t hi, lo;
        uint8_t  a;

        SMG_SET_PULLDOWN(); Delay_Us(CP_SETTLE_US);
        hi = SMG_READ();                       /* 驱动高脚 = anode */
        SMG_SET_PULLUP();   Delay_Us(CP_SETTLE_US);
        lo = (uint16_t)((~SMG_READ()) & SMG_MASK); /* 驱动低脚 = cathode */

        a = cp_single_pin(hi);
        if (a == 0) a = cp_infer_anode(lo);
        if (a) {
            for (s = 0; s < SEG_COUNT; s++)
                if (s_cp_map[s].anode == a &&
                    (lo & PIN_BIT(s_cp_map[s].cathode)))
                    s_cp_cnt[s]++;
        }
        Delay_Us(CP_GAP_US);
    }
    SMG_SET_PULLDOWN();

    if (s_cp_done >= CP_SAMPLES) { s_cp_running = 0; return 1; }
    return 0;
}

/* 整轮完成后解码累计段 -> 数字 0..100，无法解析返回 -1 */
static int16_t Power_Charlie_Decode(void)
{
    uint8_t lit[SEG_COUNT], s;
    int8_t  tens, units;
    uint8_t hundreds;
    int16_t num;

    for (s = 0; s < SEG_COUNT; s++)
        lit[s] = (s_cp_cnt[s] >= CP_LIT_MIN) ? 1 : 0;

    hundreds = (lit[SEG_B1] && lit[SEG_C1]) ? 1 : 0;
    tens  = cp_decode_digit(lit[SEG_A2], lit[SEG_B2], lit[SEG_C2], lit[SEG_D2],
                            lit[SEG_E2], lit[SEG_F2], lit[SEG_G2]);
    units = cp_decode_digit(lit[SEG_A3], lit[SEG_B3], lit[SEG_C3], lit[SEG_D3],
                            lit[SEG_E3], lit[SEG_F3], lit[SEG_G3]);

    if (units < 0) return -1;            /* 个位空白/乱码 -> 本轮无效（熄灭或采样不全） */
    if (tens  == -2) return -1;          /* 十位乱码 -> 无效 */
    if (tens  < 0) tens = 0;             /* 十位空白 = 前导零 */

    num = (int16_t)(hundreds * 100 + tens * 10 + units);
    if (num < 0) num = 0;
    if (num > 100) num = 100;
    return num;
}

/* 快速判断“整屏是否点亮”（充电闪烁检测用） */
static uint8_t Power_Charlie_Active(void)
{
    uint8_t k, act = 0;
    SMG_SET_PULLDOWN();
    for (k = 0; k < ACTIVE_SAMPLES; k++) {
        if (SMG_READ()) act = 1;         /* 有脚被驱动高 -> 正在扫描点亮 */
        Delay_Us(ACTIVE_GAP_US);
    }
    return act;
}

/* ============================================================================
 * 充电闪烁检测（输入为“整屏是否点亮”）
 * ============================================================================ */
static void Power_Detector_Update(uint8_t lit)
{
    if (lit != s_disp_lit) {
        s_last_interval = s_ms - s_last_edge_ms;
        s_last_edge_ms  = s_ms;
        s_disp_lit      = lit;
    }
}

static uint8_t Power_Detector_Classify(void)
{
    uint32_t since = s_ms - s_last_edge_ms;

    if (since > BLINK_STEADY_MS)
        return s_disp_lit ? DISP_STEADY_ON : DISP_STEADY_OFF;

    if (s_last_interval >= BLINK_SLOW_MIN && s_last_interval <= BLINK_SLOW_MAX)
        return DISP_BLINK_SLOW;          /* 0.5Hz -> 充电中 */
    if (s_last_interval >= BLINK_FAST_MIN && s_last_interval < BLINK_FAST_MAX)
        return DISP_BLINK_FAST;          /* 1Hz -> 放电 <5% */

    return s_disp_lit ? DISP_STEADY_ON : DISP_STEADY_OFF;
}

/* 周期判定：更新充电态、采纳重建结果、按需启动新一轮重建（不 TX） */
static void Power_Evaluate(void)
{
    uint8_t mode = Power_Detector_Classify();

    s_reported_charge = (mode == DISP_BLINK_SLOW) ? 1 : 0;

    if (mode == DISP_BLINK_FAST) {
        s_reported_pct = 5;                 /* <5% 危急 */
        s_have_batt = 1;
    } else if (mode == DISP_STEADY_ON || mode == DISP_BLINK_SLOW) {
        /* 采纳已完成的重建结果 */
        if (s_number_ready) {
            s_number_ready = 0;
            if (s_pending_number >= 0) {
                s_reported_pct = (uint8_t)s_pending_number;
                s_have_batt = 1;
            }
        }
        /* 空闲且处于亮相阶段则启动新一轮重建 */
        if (!s_cp_running && Power_Charlie_Active())
            Power_Charlie_Begin();
    }
    /* STEADY_OFF：保持上次电量 */
}

/* ============================================================================
 * main
 * ============================================================================ */
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    Power_UART_Init();
    Power_SMG_Init();

    while (1) {
        /* 心跳由 USART1 中断即时应答，主循环不再处理，也不做任何 TX */

        /* 闪烁检测：快速判定整屏是否点亮 */
        Power_Detector_Update(Power_Charlie_Active());

        /* 分块推进数字重建（每圈一小批，非阻塞） */
        if (Power_Charlie_Step()) {
            s_pending_number = Power_Charlie_Decode();
            s_number_ready = 1;
        }

        /* 周期判定充电/采纳重建结果 */
        if ((uint32_t)(s_ms - s_last_eval_ms) >= EVAL_MS) {
            s_last_eval_ms = s_ms;
            Power_Evaluate();
        }

        s_ms += LOOP_MS;
        Delay_Ms(LOOP_MS);
    }
}
