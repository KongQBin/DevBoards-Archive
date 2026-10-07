#include "sd_block.h"
#include "sd_core.h"
#include "sd_hci.h"
#include "../02_uart/uart.h"
#include "../02_uart/printk.h"
#include "../04_clock/clock.h"
#include "../05_timer/timer.h"

/* 读取一个 512 字节的扇区到内存 */
int sd_read_single_block(unsigned int sector, unsigned char *buffer) {
    hci_prepare_data_transfer(512, 1, 1);                         // 准备硬件传输环境 (512字节/块, 1块, 读方向=1)
    unsigned int arg = chk_is_sdhc() ? sector : (sector * 512);   // 判断是小卡还是大卡
    if (hci_send_cmd(0x113A, arg) != 0)         return -1;        // 发送读命令
    if (hci_wait_buffer_read_ready() != 0)      return -1;        // 等数据准备好
    hci_read_fifo_data((unsigned int *)buffer, 128);              // 读数据
    if (hci_wait_transfer_complete() != 0)      return -1;        // 等传输结束
    return 0;
}
