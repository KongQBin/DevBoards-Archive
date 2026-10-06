#pragma once
#include <stdbool.h>
/* 三星预留的上帝接口
 * 参数一：SDMMC
 * 参数二：起始扇区
 * 参数三：块数
 * 参数四：目标地址
 * 参数五：是否需要初始化SD/MMC    1=需要    2=不需要
 * 在使用usb烧录的前提下，拨钮开关不在SD/MMC一侧，故需要将第五个参数 置 1
 * 在使用sd烧录的场景下，由于时钟会被重置为高频，故应该也需要将第五个参数 置 1
 */

#define CopySDMMCtoMem(z,a,b,c,e) (((void(*)(int, unsigned int, unsigned short, unsigned int*, bool))(*((unsigned int *)0xD0037F98)))(z,a,b,c,e))
// 只要在 ddr_init 之后调用这一句，SD 卡第 100 扇区开始的 1024 个扇区(512KB) 就会被瞬间复制到 0x20000000！
// CopySDMMCtoMem(2, 100, 1024, (unsigned int *)0x20000000, 1);
// printk("\r\n[Verify] Payload copied to 0x20000000. HexDump:\r\n");
// unsigned int *payload_ptr = (unsigned int *)0x20000000;
// /* 打印前 16 个字 (64 Bytes) 的内存转储 */
// for (int i = 0; i < 16; i++) {
//     if (i % 4 == 0) {
//         printk("\r\n[%x]: ", payload_ptr + i); // 打印当前行的物理地址
//     }
//     printk("%x ", payload_ptr[i]);           // 打印 32-bit 数据
// }
// printk("\r\n\r\n[Verify] Parsing as ASCII String:\r\n");
// printk("-> %s\r\n", (char *)0x20000000);



/*
 * 自实现的SD驱动
 * 返回
 *  0   成功
 *  -1  失败
 */
int init_sd();
/* 读取一个 512 字节的扇区到内存 */
int sd_read_single_block(unsigned int sector, unsigned char *buffer);
