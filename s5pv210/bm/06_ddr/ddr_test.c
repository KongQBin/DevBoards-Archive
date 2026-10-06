#include "ddr_test.h"
#include "../02_uart/uart.h"
#include "../02_uart/printk.h"
#include "../05_timer/timer.h"

static inline int test_value(volatile unsigned int *addr, unsigned int write_value)
{
    *addr = write_value;               // 写入
    unsigned int read_value = *addr;   // 立即读回
    if (read_value != write_value) {
        uart2_puts("DDR DATA ERROR!\r\n");
        return -1;
    }
    return 0;
}


int ddr_test(void)
{
    int i, j;
    /* 1. 定义要探测的边界地址和内部采样点 */
    volatile unsigned int *test_addrs[] = {
        // DMC0
        (volatile unsigned int *)0x20000000,
        (volatile unsigned int *)0x20000100,
        (volatile unsigned int *)0x20100000,
        (volatile unsigned int *)0x21000000,
        (volatile unsigned int *)0x22000000,
        (volatile unsigned int *)0x24000000,
        (volatile unsigned int *)0x28000000,
        (volatile unsigned int *)0x2FFFFF00,  // 256MB 的末尾边界

        // DMC1
        (volatile unsigned int *)0x40000000,
        (volatile unsigned int *)0x40000100,
        (volatile unsigned int *)0x40100000,
        (volatile unsigned int *)0x41000000,
        (volatile unsigned int *)0x42000000,
        (volatile unsigned int *)0x44000000,
        (volatile unsigned int *)0x48000000,
        (volatile unsigned int *)0x4FFFFF00   // 256MB 的末尾边界
    };

    /* 2. 定义具有物理诊断意义的特征数据 (Patterns) */
    unsigned int test_patterns[] = {
        0x00000000, // 测漏电 (无法保持低电平)
        0xFFFFFFFF, // 测漏电 (无法保持高电平)
        0xAA55AA55, // 测相邻数据线短路 (0/1交替)
        0x55AA55AA  // 测相邻数据线短路 (反向0/1交替)
    };
    uart2_puts("\r\n========================================\r\n");
    uart2_puts("Testing DDR2...\r\n");
    uart2_puts("Range: 0x20000000 ~ 0x2FFFFFFF && 0x40000000 ~ 0x4FFFFFFF\r\n");

    /* 阶段 1：特征点多模式交叉测试 */
    for (i = 0; i < sizeof(test_patterns)/sizeof(test_patterns[0]); i++) {
        printk("Testing Pattern %x ...\r\n",test_patterns[i]);
        for (j = 0; j < sizeof(test_addrs)/sizeof(test_addrs[0]); j++) {
            if (test_value(test_addrs[j], test_patterns[i]) < 0) {
                return -1;
            }
        }
    }
    /* 阶段 2：连续增量数据测试 (Burst Test) */
    uart2_puts("Pattern 5: incremental data\r\n");
    // 写入
    for (i = 0; i < 256; i++) {
        volatile unsigned int *p = (volatile unsigned int *)(0x20000000 + i * 4);
        *p = 0x12340000 + i;
        volatile unsigned int *p2 = (volatile unsigned int *)(0x40000000 + i * 4);
        *p2 = 0x12340000 + i;
    }
    // 校验
    for (i = 0; i < 256; i++) {
        volatile unsigned int *p = (volatile unsigned int *)(0x20000000 + i * 4);
        if (*p != (0x12340000 + i)) {
            uart2_puts("DDR_0 TEST FAILED on incremental data!\r\n");
            return -1;
        }
        volatile unsigned int *p2 = (volatile unsigned int *)(0x40000000 + i * 4);
        if (*p2 != (0x12340000 + i)) {
            uart2_puts("DDR_1 TEST FAILED on incremental data!\r\n");
            return -1;
        }
    }
    uart2_puts("DDR TEST PASSED!\r\n");
    uart2_puts("DDR2 DMC0 & DMC1 is working correctly.\r\n");
    uart2_puts("========================================\r\n");
    return 0;
}
