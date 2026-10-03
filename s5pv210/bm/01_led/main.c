#include "../02_uart/uart.h"

/* ----------------------------------------------------
 * GPIO 硬件寄存器映射
 * ---------------------------------------------------- */
#define GPJ0CON     (*(volatile unsigned int *)0xE0200240)
#define GPJ0DAT     (*(volatile unsigned int *)0xE0200244)
#define GPD0CON     (*(volatile unsigned int *)0xE02000A0)
#define GPD0DAT     (*(volatile unsigned int *)0xE02000A4)

/* 软件延时函数 */
void delay(volatile unsigned int time) {
    while (time--) {
        // 汇编级的空循环，保持 CPU 消耗
    }
}

int main(void) {
    /* 1. 初始化串口硬件 */
    uart2_init();

    /* 2. 配置 LED 相关的 GPIO 为输出模式 (Output: 0001) */
    // 配置 GPJ0_3, GPJ0_4, GPJ0_5
    GPJ0CON &= ~(0xFFF << 12);  // 清空 [23:12]
    GPJ0CON |=  (0x111 << 12);  // 填入 0001 0001 0001

    // 配置 GPD0_1
    GPD0CON &= ~(0xF << 4);     // 清空 [7:4]
    GPD0CON |=  (0x1 << 4);     // 填入 0001

    uart2_puts("LED C Environment Ready!\r\n");

    /* 3. 进入主循环：闪烁与打印 */
    while (1) {
        // --- 点亮 LED (输出低电平 0) ---
        GPJ0DAT &= ~(0x7 << 3); // 清空 bit 3, 4, 5
        GPD0DAT &= ~(0x1 << 1); // 清空 bit 1

        // 串口高频互动
        uart2_puts("Blink~\r\n");

        delay(0x50000);

        // --- 熄灭 LED (输出高电平 1) ---
        GPJ0DAT |= (0x7 << 3);
        GPD0DAT |= (0x1 << 1);

        delay(0x50000);
    }

    return 0; // 永远不会到达
}
