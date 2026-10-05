#include "../01_led/led.h"
#include "../02_uart/uart.h"
#include "buzzer.h"

static void delay(volatile unsigned int time) {
    while (time--) {
    }
}

void work()
{
    uart2_puts("Beep: High pitch...\r\n");
    // 模拟高音 (延时短，频率高)
    software_beep(0x200, 2000);
    delay(0x100000); // 停顿一下
    uart2_puts("Beep: Low pitch...\r\n");
    // 模拟低音 (延时长，频率低)
    software_beep(0x800, 1000);
    delay(0x400000); // 停顿久一点
}
void test()
{
    work();
}

int main(void) {
    led_init();
    uart2_init();
    buzzer_init();

    uart2_puts("\r\n================================\r\n");
    uart2_puts(" Software PWM Buzzer Test\r\n");
    uart2_puts("================================\r\n");

    blink_loop(test,5);
    while (1) {
      work();
    }
    return 0;
}
