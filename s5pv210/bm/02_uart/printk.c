#include "uart.h"
#include "printk.h"
#include <stdarg.h>

/* 打印十进制整数 (%d) */
static void print_int(int num) {
    if (num == 0) {
        uart2_putc('0');
        return;
    }
    if (num < 0) {
        uart2_putc('-');
        num = -num;
    }

    char buf[12]; // 32位整数最多10位数，加符号和结尾足够了
    int i = 0;

    // 提取各位数字（此时是倒序的）
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }

    // 倒序输出
    while (i > 0) {
        uart2_putc(buf[--i]);
    }
}

/* 打印十六进制整数 (%x) */
static void print_hex(unsigned int num) {
    if (num == 0) {
        uart2_puts("0x0");
        return;
    }

    char buf[10];
    int i = 0;

    while (num > 0) {
        int rem = num % 16;
        // 0-9 映射到 '0'-'9'，10-15 映射到 'a'-'f'
        buf[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        num /= 16;
    }

    uart2_puts("0x");
    while (i > 0) {
        uart2_putc(buf[--i]);
    }
}

/* 核心输出函数 */
void printk(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt); // args 开始接管 fmt 之后的参数

    while (*fmt != '\0') {
        if (*fmt == '%') {
            fmt++; // 跳过 '%'
            switch (*fmt) {
                case 'd': {
                    int val = va_arg(args, int);
                    print_int(val);
                    break;
                }
                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    print_hex(val);
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char *);
                    if (!str) str = "(null)";
                    uart2_puts(str);
                    break;
                }
                case 'c': {
                    // char 会被自动提升为 int
                    char c = (char)va_arg(args, int);
                    uart2_putc(c);
                    break;
                }
                case '%': {
                    uart2_putc('%');
                    break;
                }
                default: {
                    // 未知格式原样输出
                    uart2_putc('%');
                    uart2_putc(*fmt);
                    break;
                }
            }
        } else {
            // 普通字符直接输出
            uart2_putc(*fmt);
        }
        fmt++;
    }

    va_end(args);
}
