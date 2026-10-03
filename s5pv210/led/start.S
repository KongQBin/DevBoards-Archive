/*
 *朱有鹏 X210
 *
 * LED:
 *   D22 -> GPJ0_3
 *   D23 -> GPJ0_4
 *   D24 -> GPJ0_5
 *   D25 -> GPD0_1 / PWMTOUT1
 *
 * LED 为低电平点亮
 */

#define WTCON       0xE2700000

#define GPJ0CON     0xE0200240
#define GPJ0DAT     0xE0200244

#define GPD0CON     0xE02000A0
#define GPD0DAT     0xE02000A4


.global _start

_start:

     # 关闭 watchdog
    ldr r0, =WTCON
    mov r1, #0
    str r1, [r0]


    /*
     * --------------------------------------------------------
     * 配置 GPJ0_3 / GPJ0_4 / GPJ0_5 为 GPIO 输出
     *
     * GPJ0CON:
     *
     * GPJ0_3 -> bit[15:12]
     * GPJ0_4 -> bit[19:16]
     * GPJ0_5 -> bit[23:20]
     *
     * GPIO output = 0001
     *
     * 因此：
     *
     *   GPJ0_3 = 0001
     *   GPJ0_4 = 0001
     *   GPJ0_5 = 0001
     *
     * => 0x00111000
     * --------------------------------------------------------
     */

    ldr r0, =GPJ0CON
    ldr r1, [r0]

    /* 清除 GPJ0_3 ~ GPJ0_5 原来的功能配置 */
    ldr r2, =0x00FFF000
    bic r1, r1, r2

    /* 设置为 GPIO Output */
    ldr r2, =0x00111000
    orr r1, r1, r2

    str r1, [r0]


    /*
     * --------------------------------------------------------
     * 配置 GPD0_1 为 GPIO 输出
     *
     * GPD0CON[1] -> bit[7:4]
     * 0001 = GPIO Output
     *
     * 所以设置 bit[7:4] = 0001
     * 即 0x00000010
     * --------------------------------------------------------
     */

    ldr r0, =GPD0CON
    ldr r1, [r0]

    /* 清除 GPD0_1 的功能配置 */
    ldr r2, =0x000000F0
    bic r1, r1, r2

    /* 设置 GPD0_1 为 GPIO Output */
    ldr r2, =0x00000010
    orr r1, r1, r2

    str r1, [r0]


loop:
     # 点亮 D22 / D23 / D24
     # GPJ0_3 / 4 / 5 输出低电平
    ldr r0, =GPJ0DAT
    ldr r1, [r0]
    ldr r2, =0x00000038
    bic r1, r1, r2
    str r1, [r0]


     # 点亮 D25
     # GPD0_1 输出低电平
    ldr r0, =GPD0DAT
    ldr r1, [r0]
    mov r2, #0x02
    bic r1, r1, r2
    str r1, [r0]

     # 延时
    ldr r2, =0x400000

delay_on:
    subs r2, r2, #1
    bne delay_on

     # 熄灭 D22 / D23 / D24
    ldr r0, =GPJ0DAT
    ldr r1, [r0]
    ldr r2, =0x00000038
    orr r1, r1, r2
    str r1, [r0]

     # 熄灭 D25
    ldr r0, =GPD0DAT
    ldr r1, [r0]
    mov r2, #0x02
    orr r1, r1, r2
    str r1, [r0]

     # 延时
    ldr r2, =0x400000

delay_off:
    subs r2, r2, #1
    bne delay_off

     # 无限循环
    b loop
