#pragma once
void uart2_init(void);
void uart2_putc(char c);
void uart2_puts(const char *str);
char uart2_getc(void);
