#include "buzzer.h"
#include "../05_timer/timer.h"

#define GPD0CON (*(volatile unsigned int *)0xE02000A0)
#define GPD0DAT (*(volatile unsigned int *)0xE02000A4)

void buzzer_init(int soft) {
    /* 配置 GPD0_2 为普通 GPIO 输出模式 (0001) */
    GPD0CON &= ~(0xF << 8); // 清空 bit[11:8]
    if(soft)
      GPD0CON |=  (0x1 << 8); // 设为 0001
    else
      GPD0CON |=  (0x2 << 8);
}

void buzzer_on(void) {
    /* 蜂鸣器引脚拉高 */
    GPD0DAT |= (1 << 2);
}

void buzzer_off(void) {
    /* 蜂鸣器引脚拉低 */
    GPD0DAT &= ~(1 << 2);
}

/* 暴露给业务层的发声 API */
void buzzer_play(unsigned int freq) {
    // 蜂鸣器硬件接在 Timer 2 上，所以这里写死传参 2
    pwm_set_freq(2, freq);
}
