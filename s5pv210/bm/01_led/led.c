#include "led.h"
/* ----------------------------------------------------
 * GPIO 硬件寄存器映射
 * ---------------------------------------------------- */
#define GPJ0CON     (*(volatile unsigned int *)0xE0200240)
#define GPJ0DAT     (*(volatile unsigned int *)0xE0200244)
#define GPD0CON     (*(volatile unsigned int *)0xE02000A0)
#define GPD0DAT     (*(volatile unsigned int *)0xE02000A4)

void led_init()
{
    /* 配置 LED 相关的 GPIO 为输出模式 (Output: 0001) */
    // 配置 GPJ0_3, GPJ0_4, GPJ0_5
    GPJ0CON &= ~(0xFFF << 12);  // 清空 [23:12]
    GPJ0CON |=  (0x111 << 12);  // 填入 0001 0001 0001

    // 配置 GPD0_1
    GPD0CON &= ~(0xF << 4);     // 清空 [7:4]
    GPD0CON |=  (0x1 << 4);     // 填入 0001

    // 默认上电全灭 (高电平)
    GPJ0DAT |= (0x7 << 3);
    GPD0DAT |= (0x1 << 1);
}

/* 2. 导出给业务层的 API：用一个函数控制 4 个灯的独立亮灭 */
void led_set_state(int led_id, int state) {
    if (led_id >= 0 && led_id <= 2) {
        int shift = led_id + 3; // LED0~2 对应 bit 3,4,5
        if (state) GPJ0DAT &= ~(1 << shift); // 亮 (拉低)
        else       GPJ0DAT |=  (1 << shift); // 灭 (拉高)
    } else if (led_id == 3) {
        if (state) GPD0DAT &= ~(1 << 1);
        else       GPD0DAT |=  (1 << 1);
    }
}

