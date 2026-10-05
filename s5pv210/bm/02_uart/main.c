#include "uart.h"
#include "../01_led/led.h"

int main(void) {
    led_init();
    /* 初始化 UART2 硬件 */
    uart2_init();

    /* 打印开机信息 */
    uart2_puts("\r\n============================================\r\n");
    uart2_puts("  Hello, Rocky Linux!\r\n");
    uart2_puts("  S5PV210 BareMetal C Environment is Ready!\r\n");
    uart2_puts("============================================\r\n");
    uart2_puts("Please type something on your keyboard:\r\n");
    for(int i=0;i<4;++i)
      led_set_state(i,1);

    /* 进入无限循环：接收并回显字符 */
    while (1) {
        char c = uart2_getc(); // 阻塞等待键盘输入
        uart2_putc(c);         // 立刻把收到的字符发回电脑终端显示
    }

    return 0; // 永远不会执行到这里
}
