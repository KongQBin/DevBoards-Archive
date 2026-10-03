#include "uart.h"

/* GPIO 寄存器 */
#define GPA1CON     (*(volatile unsigned int *)0xE0200020)

/* UART2 寄存器 */
#define UART2_BASE  0xE2900800
#define ULCON2      (*(volatile unsigned int *)(UART2_BASE + 0x00))
#define UCON2       (*(volatile unsigned int *)(UART2_BASE + 0x04))
#define UFCON2      (*(volatile unsigned int *)(UART2_BASE + 0x08))
#define UMCON2      (*(volatile unsigned int *)(UART2_BASE + 0x0C))
#define UTRSTAT2    (*(volatile unsigned int *)(UART2_BASE + 0x10)) // 缓冲区标志
#define UTXH2       (*(volatile unsigned int *)(UART2_BASE + 0x20)) // 发送缓冲区
#define URXH2       (*(volatile unsigned int *)(UART2_BASE + 0x24))
#define UBRDIV2     (*(volatile unsigned int *)(UART2_BASE + 0x28))
#define UDIVSLOT2   (*(volatile unsigned int *)(UART2_BASE + 0x2C))

void uart2_init(void) {
    /* 1. 配置 GPA1_0 和 GPA1_1 为 UART2 的 RXD 和 TXD (功能模式 2) */
    GPA1CON &= ~(0xFF << 0);     // 清除 [7:0]
    GPA1CON |= (0x22 << 0);      // 设置为 0x22

    /* 2. 配置 UART2 线控寄存器 (8N1: 8个数据位, 无校验, 1个停止位) */
    ULCON2 = 0x03;

    /* 3. 配置 UART2 控制寄存器 (开启轮询/中断模式的发送和接收) */
    UCON2 = 0x05;

    /* 4. 禁用 FIFO 和 调制解调器控制，使用最简单的轮询传输 */
    UFCON2 = 0x00;
    UMCON2 = 0x00;

    /* 5. 配置波特率为 115200 (PCLK = 66MHz) */
    UBRDIV2 = 34;
    UDIVSLOT2 = 0xDFDD;
}

void uart2_putc(char c) {
    /* 如果是回车，自动补上换行符，以便终端显示正常 */
    if (c == '\n') {
        uart2_putc('\r');
    }

    /* 轮询 UTRSTAT2 的第 1 位 (发送缓冲区为空标志) */
    while (!(UTRSTAT2 & (1 << 1)));
    UTXH2 = c;
}

void uart2_puts(const char *str) {
    while (*str) {
        uart2_putc(*str++);
    }
}

char uart2_getc(void) {
    /* 轮询 UTRSTAT2 的第 0 位 (接收缓冲区有数据标志) */
    while (!(UTRSTAT2 & (1 << 0)));

    /* 返回接收到的字符 */
    return URXH2;
}
