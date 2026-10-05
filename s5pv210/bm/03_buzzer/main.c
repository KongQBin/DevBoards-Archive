#include "../01_led/led.h"
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
    led_init();
    uart2_init();
    buzzer_init(1);

    uart2_puts("\r\n================================\r\n");
    uart2_puts(" Software PWM Buzzer Test\r\n");
    uart2_puts("================================\r\n");
    for(int i=0;i<4;++i)
      led_set_state(i,1);

    while (1) {
      uart2_puts("Beep: High pitch...\r\n");
      // 模拟高音 (延时短，频率高)
      software_beep(0x200, 1000);
      delay(0x1000); // 停顿一下
      uart2_puts("Beep: Low pitch...\r\n");
      // 模拟低音 (延时长，频率低)
      software_beep(0x800, 500);
      delay(0x4000); // 停顿久一点
    }
    return 0;
}
