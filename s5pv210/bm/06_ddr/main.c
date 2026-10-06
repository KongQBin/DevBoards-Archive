#include "../01_led/led.h"
#include "../02_uart/uart.h"
#include "../02_uart/printk.h"
#include "../03_buzzer/buzzer.h"
#include "../04_clock/clock.h"
#include "../05_timer/timer.h"
#include "ddr.h"

int main(void) {
    led_init();
    clock_init();       // 挂上 1GHz 主频和 66MHz 的 PCLK_PSYS
    uart2_init();
    timer_base_init();  // 初始化 PWM 定时器硬件

    printk("\r\nHardware PWM Timer Activated!\r\n");

    buzzer_init(0);
    /* 标准音阶频率表 (Do, Re, Mi, Fa, So, La, Xi) */
    unsigned int notes[] = {262,/* 294,*/ 330,/* 349,*/ 392,/* 440,*/ 494};
    printk("Playing Scale...\r\n");
    for (int i = 0; i < sizeof(notes)/sizeof(unsigned int); i++) {
        buzzer_play(notes[i]);  // 硬件精准输出音调
        delay_ms(200);          // 持续 200 毫秒
    }
    buzzer_play(0); // 停止发声


    printk("\r\nDDR initing~\r\n");
    ddr_init();
    printk("\r\nDDR init finish!\r\n");
    ddr_test();
    return 0;
}
