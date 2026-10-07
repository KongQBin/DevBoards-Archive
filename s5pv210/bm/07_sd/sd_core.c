#include "sd_core.h"
#include "sd_hci.h"
#include "../02_uart/uart.h"
#include "../02_uart/printk.h"
#include "../04_clock/clock.h"
#include "../05_timer/timer.h"


/* CMD6 */
#define SD_CMD6_CHECK_HS_ARG       0x00FFFFF1U
#define SD_CMD6_SWITCH_HS_ARG      0x80FFFFF1U
/* R1 */
#define SD_R1_ILLEGAL_COMMAND      (1U << 22)

int is_sdhc = 0; // 0代表小容量卡(SDSC)，1代表大容量卡(SDHC)
int chk_is_sdhc()
{
  return is_sdhc;
}

unsigned int rca = 0; // 全局变量，保存卡的“工号”
static inline int sd_set_bus_width_4bit(void)
{
    /* ACMD6 必须先 CMD55 */
    if (hci_send_cmd(0x371A, (rca << 16)) != 0) {
        printk("[ERROR] CMD55 before ACMD6 failed.\r\n");
        return -1;
    }
    /* ACMD6 argument: 2 = 4-bit */
    if (hci_send_cmd(0x061A, 0x00000002) != 0) {
        printk("[ERROR] ACMD6 SET_BUS_WIDTH failed.\r\n");
        return -1;
    }
    /* 检查卡片是否拒绝（调用纯净响应接口） */
    if (hci_get_response_0() & SD_R1_ILLEGAL_COMMAND) {
        printk("[ERROR] Card rejected ACMD6.\r\n");
        return -1;
    }
    /* 卡片已经切成 4-bit，命令主板也切过去 */
    hci_set_bus_width_4bit();
    printk("4-bit Bus Mode Enabled.\r\n");
    return 0;
}

static inline int sd_card_handshake(void) {
    printk("Starting SD Card Handshake...\r\n");
    /* 步骤 1：发送 CMD0 (GO_IDLE_STATE) */
    hci_send_cmd(0x0000, 0x00000000);
    printk("CMD0 sent. Card reset.\r\n");
    /* 步骤 2：发送 CMD8 (SEND_IF_COND) */
    hci_send_cmd(0x081A, 0x000001AA);
    // 使用 HCI 接口获取响应，斩断寄存器依赖
    if ((hci_get_response_0() & 0xFFF) != 0x1AA) {
        printk("CMD8 Failed! Not a valid SD2.0 Card.\r\n");
        return -1;
    }
    printk("CMD8 Passed! SD2.0 Card detected.\r\n");
    /* 步骤 3：发送 ACMD41 循环询问 */
    unsigned int response;
    int timeout = 10000;
    do {
        if (hci_send_cmd(0x371A, 0x00000000) == 0) {
            if (hci_send_cmd(0x2902, 0x40300000) == 0) {
                // 使用 HCI 接口获取响应
                response = hci_get_response_0();
                if (response & (1 << 31)) {
                    if (response & (1 << 30)) {
                        is_sdhc = 1;
                        printk("Card is SDHC/SDXC (Block Addressing)\r\n");
                    } else {
                        is_sdhc = 0;
                        printk("Card is SDSC (Byte Addressing)\r\n");
                    }
                    break;
                }
            }
        }
        timeout--;
        delay_ms(10);
    } while (timeout > 0);
    if (timeout == 0) {
        printk("ACMD41 Timeout! Card is dead.\r\n");
        return -1;
    }
    printk("ACMD41 Passed! SD Card is Ready for Action.\r\n");
    return 0;
}

static inline int sd_card_identify(void) {
    printk("Identifying Card...\r\n");
    /* 1. 发送 CMD2 */
    hci_send_cmd(0x0209, 0x00000000);
    printk("CMD2 Passed.\r\n");
    /* 2. 发送 CMD3 */
    hci_send_cmd(0x031A, 0x00000000);
    // 使用 HCI 接口获取 RCA
    rca = (hci_get_response_0() >> 16) & 0xFFFF;
    printk("CMD3 Passed. Card RCA is: %x\r\n", rca);
    /* 3. 发送 CMD7 */
    hci_send_cmd(0x071A, (rca << 16));
    delay_ms(20);
    printk("CMD7 Passed. Card selected and in Transfer State!\r\n");
    return 0;
}


/*
 * CMD6 是一个带数据阶段的命令
 *
 * 和 CMD17 不同：
 *   CMD17 -> 返回 512 字节扇区
 *   CMD6  -> 返回 64 字节 Switch Function Status
 * CMD6:
 *   R1 响应 + DAT0~3 返回 512 bit = 64 bytes
 */
static int sd_cmd6_transfer(unsigned int arg, unsigned char *status)
{
    unsigned int *p = (unsigned int *)status;
    // 准备硬件传输环境 (CMD6 返回固定 64 字节数据/块, 1块, 读方向=1)
    hci_prepare_data_transfer(64, 1, 1);
    /*
     * CMD6:
     * index = 6 -> 0x0600
     * 0x3A:
     *   bit5 = Data Present
     *   bit4 = Check Command Index
     *   bit3 = Check CRC
     *   bit1 = 48-bit Response
     * 所以：
     *   CMD6 = 0x063A
     */
    if (hci_send_cmd(0x063A, arg) != 0) return -1;
    // CMD6 如果不被支持，有可能正常收到 R1，但 R1 的 ILLEGAL_COMMAND 会被置位
    unsigned int r1_resp = hci_get_response_0();
    if (r1_resp & SD_R1_ILLEGAL_COMMAND) {
        printk("[CMD6] Illegal command. R1=%x\r\n", r1_resp);
        return -1;
    }
    // 等待 Buffer Read Ready
    if(hci_wait_buffer_read_ready() !=0 ) return -1;
    /*
     * 64 bytes / 4 = 16 words
     * S5PV210 BDATA 是 32-bit FIFO
     * 按这种方式直接写入 status[] 后，
     * status[0]...status[63] 就是 CMD6 status 的字节顺序
     */
    hci_read_fifo_data((unsigned int *)p, 16);
    // 等待整个 data transaction 完成
    if(hci_wait_transfer_complete())  return -1;
    return 0;
}

/*
 * 尝试把 SD 卡合法切换到 High-Speed 模式
 * 返回值：
 *   1  = 成功进入 High-Speed，4-bit / 48MHz
 *   0  = 卡不支持 HS，已回退到 Default Speed，4-bit / 24MHz
 *  -1  = 真正的初始化/通信错误
 */
static inline int sd_set_high_speed(void)
{
    unsigned int status_words[16];
    unsigned char *status = (unsigned char *)status_words;
    /* Step 1: 先切换到 4-bit bus */
    if (sd_set_bus_width_4bit() != 0) {
        printk("[ERROR] Cannot enable 4-bit SD bus.\r\n");
        return -1;
    }

    /* Step 2: CMD6 Mode 0: CHECK FUNCTION */
    printk("Checking SD High-Speed capability...\r\n");
    if (sd_cmd6_transfer(SD_CMD6_CHECK_HS_ARG, status) != 0) {
        printk("CMD6 Check failed. Falling back to Default Speed.\r\n");
        if (hci_set_default_speed() != 0) return -1;
        printk("SD Default Speed enabled: 4-bit, 24MHz.\r\n");
        return 0;
    }

    /* 检查是否支持 High-Speed */
    printk("CMD6 Check: Group1 support = %x\r\n", status[13]);
    if (!(status[13] & (1 << 1))) {
        printk("Card does NOT support SD High-Speed. Using Default Speed.\r\n");
        if (hci_set_default_speed() != 0) return -1;
        printk("SD Default Speed enabled: 4-bit, 24MHz.\r\n");
        return 0;
    }
    printk("Card supports SD High-Speed.\r\n");

    /* Step 3: CMD6 Mode 1: SWITCH FUNCTION */
    printk("Switching card to High-Speed mode...\r\n");
    if (sd_cmd6_transfer(SD_CMD6_SWITCH_HS_ARG, status) != 0) {
        printk("[ERROR] CMD6 High-Speed switch failed.\r\n");
        return -1;
    }

    /* 检查卡片是否同意切换 */
    printk("CMD6 Switch: selected Group1 function = %x\r\n", status[16] & 0x0F);
    if ((status[16] & 0x0F) != 1) {
        printk("Card refused High-Speed switch. Using Default Speed.\r\n");
        if (hci_set_default_speed() != 0) return -1;
        printk("SD Default Speed enabled: 4-bit, 24MHz.\r\n");
        return 0;
    }

    /* 给卡片一点时间完成内部时序切换 */
    delay_ms(1);

    /* Step 4: 卡片已经进入高速，命令主板全速运转！ */
    if (hci_set_high_speed() != 0) {
        printk("[ERROR] Failed to set Host Controller to High Speed.\r\n");
        return -1;
    }
    printk("SD High-Speed Enabled: 4-bit, 48MHz.\r\n");
    return 1;
}

int init_sd()
{
    int speed_mode;
    hci_controller_init(); // 底层硬件初始化
    if (sd_card_handshake() != 0) return -1;
    if (sd_card_identify() != 0)  return -1;
    speed_mode = sd_set_high_speed();
    if (speed_mode < 0) {
        printk("[ERROR] SD transfer mode setup failed.\r\n");
        return -1;
    }
    if (speed_mode == 1) {
        printk("SD final mode: High-Speed 48MHz / 4-bit\r\n");
    } else {
        printk("SD final mode: Default-Speed 24MHz / 4-bit\r\n");
    }
    return 0;
}
