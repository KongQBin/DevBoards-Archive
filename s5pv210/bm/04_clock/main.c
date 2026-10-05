#include "../01_led/led.h"
#include "../02_uart/uart.h"
#include "../03_buzzer/buzzer.h"
#include "clock.h"

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
    // 上电默认时钟 (靠 iROM 遗留的时钟运行)
    led_init();
    uart2_init();
    buzzer_init(1);

    uart2_puts("\r\n--- Before Overclocking ---\r\n");
    uart2_puts("Beep once slowly...\r\n");


    for(int j=0;j<1;++j)
    {
        delay(1000);
        software_beep(0xF00, 100);
        for(int i=0;i<4;++i)
          led_set_state(i,1);
        delay(1000);
        software_beep(0x800, 200);
        for(int i=0;i<4;++i)
          led_set_state(i,0);
        delay(1000);
        software_beep(0x400, 200);
    }

    // to 1GHz
    uart2_puts("Ignition! Overclocking to 1GHz...\r\n");
    clock_init();

    /* 超频后，UART 的波特率实际上会崩溃 (因为 PCLK_PSYS 从 66M 被我们重新洗牌了)，
     * 但因为我们刚才恰好把 PCLK_PSYS 也配成了 66MHz(133M/2，详见 DIV0 配置)，
     * 所以这里 UART 奇迹般地依然能正常输出！*/
    uart2_init();

    uart2_puts("--- After Overclocking ---\r\n");
    uart2_puts("System running at 1000MHz!\r\n");

    while (1) {
      /* 仔细听：因为 CPU 快了近 3 倍，同样的 0x200000 循环现在只用极短的时间就跑完了。
       * 蜂鸣器的长音变成了极其急促的短音！ */
      for(int j=0;j<1;++j)
      {
          delay(1000);
          software_beep(0xF00, 100);
          for(int i=0;i<4;++i)
            led_set_state(i,1);
          delay(1000);
          software_beep(0x800, 200);
          for(int i=0;i<4;++i)
            led_set_state(i,0);
          delay(1000);
          software_beep(0x400, 200);
      }
      uart2_puts("continue\r\n");
    }
    return 0;
}
