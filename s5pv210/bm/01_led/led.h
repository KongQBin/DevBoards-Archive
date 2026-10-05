#pragma once
void led_init();
// led_id: 0~3
// 0 -> D4
// 1 -> D5
// 2 -> D6
// 3 -> D7
// state: 1(亮), 0(灭)
void led_set_state(int led_id, int state);

