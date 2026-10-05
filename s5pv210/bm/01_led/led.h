#pragma once
void led_init();
typedef void (*load_cb)();
void blink_loop(load_cb cb, int loop_num);
