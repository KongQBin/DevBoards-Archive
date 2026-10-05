#include "led.h"

typedef void (*load_cb)();
/* 软件延时函数 */
void delay(volatile unsigned int time) {
    while (time--) {
        // 汇编级的空循环，保持 CPU 消耗
    }
}

void blink_loop(load_cb cb, int loop_num)
{
    /* 进入主循环：闪烁 */
    // 0、正数 = 次数
    // 负数 = 死循环
    while (loop_num--) {
        if(cb) cb();
        // 点亮 LED (输出低电平 0)
        for(int i=0;i<4;++i)
          led_set_state(i,1);
        delay(0x50000);
        // 熄灭 LED (输出高电平 1)
        for(int i=0;i<4;++i)
          led_set_state(i,0);
        delay(0x50000);
    }
}

int main(void) {
  led_init();
  blink_loop(0,-1);
  return 0; // 永远不会到达
}
