#include "led.h"

int main(void) {
  led_init();
  blink_loop(0,0);
  return 0; // 永远不会到达
}
