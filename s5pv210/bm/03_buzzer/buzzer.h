#pragma conce

void buzzer_init(int soft);

// 软模拟蜂鸣器
void buzzer_on(void);
void buzzer_off(void);

// 定时器驱动蜂鸣器
void buzzer_play(unsigned int freq);
