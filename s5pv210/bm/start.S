#define WTCON 0xE2700000

/* 单独划分一个 section */
.section .text.entry

.global _start
_start:
    /* 1. 关闭看门狗 */
    ldr r0, =WTCON
    mov r1, #0
    str r1, [r0]

    /* 2. 初始化栈指针 (Stack Pointer)
     * S5PV210 内部 SRAM 末端安全可用地址 */
    ldr sp, =0xD0037D80

    /* 3. 跳转到 C 语言入口函数 */
    bl main

    /* 4. 兜底死循环 */
halt:
    b halt
