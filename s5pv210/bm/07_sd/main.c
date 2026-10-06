#include "../01_led/led.h"
#include "../02_uart/uart.h"
#include "../02_uart/printk.h"
#include "../03_buzzer/buzzer.h"
#include "../04_clock/clock.h"
#include "../05_timer/timer.h"
#include "../06_ddr/ddr.h"
#include "../06_ddr/ddr_test.h"
#include "sd.h"


int main(void) {
    led_init();
    clock_init();       // 挂上 1GHz 主频和 66MHz 的 PCLK_PSYS
    uart2_init();
    timer_base_init();  // 初始化 PWM 定时器硬件

    printk("\r\nHardware PWM Timer Activated!\r\n");

    // buzzer_init(0);
    // /* 标准音阶频率表 (Do, Re, Mi, Fa, So, La, Xi) */
    // unsigned int notes[] = {262,/* 294,*/ 330,/* 349,*/ 392,/* 440,*/ 494};
    // printk("Playing Scale...\r\n");
    // for (int i = 0; i < sizeof(notes)/sizeof(unsigned int); i++) {
    //     buzzer_play(notes[i]);  // 硬件精准输出音调
    //     delay_ms(200);          // 持续 200 毫秒
    // }
    // buzzer_play(0); // 停止发声


    printk("\r\nDDR initing~\r\n");
    ddr_init();
    printk("\r\nDDR init finish!\r\n");
    if(ddr_test()) while(1);
    printk("\r\nDDR test finish!\r\n");

    if (!init_sd())
    {
        printk("Loading Str Payload to DDR...\r\n");
        unsigned char *payload_ptr = (unsigned char *)0x20000000;
        for (int i = 0; i < 1024; i++) {
            int retry = 3; // 每个扇区最多重试 3 次
            int current_sector = 100 + i;
            while (retry > 0) {
                if (sd_read_single_block(current_sector, payload_ptr + (i * 512)) == 0) {
                    break; // 读取成功，跳出重试循环，继续读下一个扇区
                }
                retry--;
                // printk("Retry Sector %d...\r\n", current_sector); // 调试用，可注释
            }
            if (retry == 0) {
                printk("\r\n[Fatal Error] SD Read Failed at Sector %d. Halting.\r\n", current_sector);
                while(1); // 硬件严重错误，停止加载
            }
            // 每加载 128 个扇区打印一个点，让用户知道没卡死
            if ((i % 128) == 0) printk(".");
        }
        printk("\r\nPayload Loading Complete! Data at 0x20000000\r\n");
    }
    printk("String: -> %s\r\n", (char *)0x20000000);
    printk("\r\nStr Loader Phase 1 Complete. Halting.\r\n");
    while(1); // 悬停在这里，防止跑飞
    return 0;
}
