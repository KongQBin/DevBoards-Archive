#include "buzzer.h"

#define GPD0CON (*(volatile unsigned int *)0xE02000A0)
#define GPD0DAT (*(volatile unsigned int *)0xE02000A4)

void buzzer_init(void) {
    /* 配置 GPD0_2 为普通 GPIO 输出模式 (0001) */
    GPD0CON &= ~(0xF << 8); // 清空 bit[11:8]
    GPD0CON |=  (0x1 << 8); // 设为 0001
}

void buzzer_on(void) {
    /* 蜂鸣器引脚拉高 */
    GPD0DAT |= (1 << 2);
}

void buzzer_off(void) {
    /* 蜂鸣器引脚拉低 */
    GPD0DAT &= ~(1 << 2);
}
