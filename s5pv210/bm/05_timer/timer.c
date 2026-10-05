#include "timer.h"

#define TCFG0   (*(volatile unsigned int *)0xE2500000)
#define TCFG1   (*(volatile unsigned int *)0xE2500004)
#define TCON    (*(volatile unsigned int *)0xE2500008)
#define TCNTB2  (*(volatile unsigned int *)0xE2500024)
#define TCMPB2  (*(volatile unsigned int *)0xE2500028)


/* 初始化定时器基准时钟 */
void timer_base_init(void) {
    // 关停幽灵定时器
    TCON &= ~(1 << 20); // 强行停止 Timer 4
    TCON &= ~(1 << 12); // 强行停止 Timer 2

    // 配置预分频和分频器 (纯粹的时钟逻辑)
    // 第一级：Prescaler 1 设为 65 (66 分频)
    TCFG0 &= ~(0xFF << 8);
    TCFG0 |=  (65   << 8);

    // 第二级：Divider
    TCFG1 &= ~(0xF << 8);  // 清空 Timer 2
    TCFG1 |=  (0   << 8);  // Timer 2 设为 1/2 分频 (500kHz)
    TCFG1 &= ~(0xF << 16); // 清空 Timer 4
    TCFG1 |=  (0   << 16); // 强行把 Timer 4 也设为 1/2 分频 (500kHz)！
}

/* 提供一个通用的 PWM 频率设置接口 */
// timer_id: 定时器编号 (0~4)
void pwm_set_freq(int timer_id, unsigned int freq) {
    if (timer_id == 2) {
        if (freq == 0) {
            TCON &= ~(1 << 12);
            return;
        }
        unsigned int tcnt = 500000 / freq;
        TCNTB2 = tcnt;
        TCMPB2 = tcnt / 2;      // 固定 50% 占空比

        TCON |= (1 << 15);
        TCON |= (1 << 13);
        TCON &= ~(1 << 13);
        TCON |= (1 << 12);
    }
    // 如果以后要控制屏幕背光(Timer 0)，就在这里扩展
}

#define TCNTB4  (*(volatile unsigned int *)0xE250003C) // Timer4 计数缓冲寄存器
#define TCNTO4  (*(volatile unsigned int *)0xE2500040) // Timer4 观察寄存器(实时读数)
/* 使用硬件定时器 4 实现绝对精准的毫秒级延时 */
void delay_ms(unsigned int ms) {
    TCNTB4 = ms * 500;      // 填入目标计数值

    TCON &= ~(1 << 22);     // 关闭 Auto-reload
    TCON |=  (1 << 21);     // 置位 Manual Update
    TCON &= ~(1 << 21);     // 立刻清零 Manual Update

    TCON |=  (1 << 20);     // 启动 Timer 4 开始倒数

    /* 【跨时钟域防抖】 */
    // 强制 1GHz 的 CPU 等一下，直到 500kHz 的慢速外设真正把数据加载进 TCNTO4
    while (TCNTO4 == 0);

    // 数据安全加载完毕，现在才开始真正等待它倒数到 0
    while (TCNTO4 != 0);

    TCON &= ~(1 << 20);     // 延时结束，关闭 Timer 4
}
