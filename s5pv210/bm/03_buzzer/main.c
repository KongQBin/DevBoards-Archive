#include "../02_uart/uart.h"
#include "buzzer.h"

void delay(volatile unsigned int time) {
    while (time--) {
    }
}

/* 软件模拟 PWM 发声函数 */
void software_beep(unsigned int pitch_delay, unsigned int duration) {
    while (duration--) {
        buzzer_on();
        delay(pitch_delay); // 控制频率（音调）
        buzzer_off();
        delay(pitch_delay); // 控制频率（音调）
    }
}

int main(void) {
    uart2_init();
    buzzer_init();

    uart2_puts("\r\n================================\r\n");
    uart2_puts(" Software PWM Buzzer Test\r\n");
    uart2_puts("================================\r\n");

    while (1) {
        uart2_puts("Beep: High pitch...\r\n");
        // 模拟高音 (延时短，频率高)
        software_beep(0x200, 2000);

        delay(0x100000); // 停顿一下

        uart2_puts("Beep: Low pitch...\r\n");
        // 模拟低音 (延时长，频率低)
        software_beep(0x800, 1000);

        delay(0x400000); // 停顿久一点
    }

    return 0;
}
