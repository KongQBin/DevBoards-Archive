#include "sd_hci.h"
#include "../02_uart/uart.h"
#include "../02_uart/printk.h"
#include "../04_clock/clock.h"
#include "../05_timer/timer.h"

#define HSMMC2_BASE         0xEB200000
/* 1. 基础配置与命令引擎 */
#define SD2_SYSAD           (*(volatile unsigned int *)  (HSMMC2_BASE + 0x00)) // SDMASYSAD2: DMA系统地址
#define SD2_BLKSIZE         (*(volatile unsigned short *)(HSMMC2_BASE + 0x04)) // BLKSIZE2: 块大小 (通常512)
#define SD2_BLKCNT          (*(volatile unsigned short *)(HSMMC2_BASE + 0x06)) // BLKCNT2: 传输多少个块
#define SD2_ARGUMENT        (*(volatile unsigned int *)  (HSMMC2_BASE + 0x08)) // ARGUMENT2: 32位命令参数
#define SD2_TRNMOD          (*(volatile unsigned short *)(HSMMC2_BASE + 0x0C)) // TRNMOD2: 传输模式配置
#define SD2_CMDREG          (*(volatile unsigned short *)(HSMMC2_BASE + 0x0E)) // CMDREG2: 16位命令发射器
/* 2. 响应接收室 */
#define SD2_RSPREG0         (*(volatile unsigned int *)  (HSMMC2_BASE + 0x10)) // RSPREG0_2: 响应寄存器0 (最常用)
#define SD2_RSPREG1         (*(volatile unsigned int *)  (HSMMC2_BASE + 0x14)) // RSPREG1_2
#define SD2_RSPREG2         (*(volatile unsigned int *)  (HSMMC2_BASE + 0x18)) // RSPREG2_2
#define SD2_RSPREG3         (*(volatile unsigned int *)  (HSMMC2_BASE + 0x1C)) // RSPREG3_2
/* 3. 数据通道 */
#define SD2_BDATA           (*(volatile unsigned int *)  (HSMMC2_BASE + 0x20)) // BDATA2: 缓冲数据读写口
/* 4. 状态与控制 (电源、时钟、复位) */
#define SD2_PRNSTS          (*(volatile unsigned int *)  (HSMMC2_BASE + 0x24)) // PRNSTS2: 当前工作状态
#define SD2_HOSTCTL         (*(volatile unsigned char *) (HSMMC2_BASE + 0x28)) // HOSTCTL2: 主机控制 (比如设为4线宽)
#define SD2_PWRCON          (*(volatile unsigned char *) (HSMMC2_BASE + 0x29)) // PWRCON2: 电源控制
#define SD2_BLKGAP          (*(volatile unsigned char *) (HSMMC2_BASE + 0x2A)) // BLKGAP2
#define SD2_WAKCON          (*(volatile unsigned char *) (HSMMC2_BASE + 0x2B)) // WAKCON2
#define SD2_CLKCON          (*(volatile unsigned short *)(HSMMC2_BASE + 0x2C)) // CLKCON2: 时钟分频与开关
#define SD2_TIMEOUTCON      (*(volatile unsigned char *) (HSMMC2_BASE + 0x2E)) // TIMEOUTCON2
#define SD2_SWRST           (*(volatile unsigned char *) (HSMMC2_BASE + 0x2F)) // SWRST2: 软件复位控制
/* 5. 中断状态机 */
#define SD2_NORINTSTS       (*(volatile unsigned short *)(HSMMC2_BASE + 0x30)) // NORINTSTS2: 正常中断标志
#define SD2_ERRINTSTS       (*(volatile unsigned short *)(HSMMC2_BASE + 0x32)) // ERRINTSTS2: 错误中断标志
#define SD2_NORINTSTSEN     (*(volatile unsigned short *)(HSMMC2_BASE + 0x34)) // NORINTSTSEN2
#define SD2_ERRINTSTSEN     (*(volatile unsigned short *)(HSMMC2_BASE + 0x36)) // ERRINTSTSEN2
#define SD2_NORINTSIGEN     (*(volatile unsigned short *)(HSMMC2_BASE + 0x38)) // NORINTSIGEN2
#define SD2_ERRINTSIGEN     (*(volatile unsigned short *)(HSMMC2_BASE + 0x3A)) // ERRINTSIGEN2
/* 6. 高级特性 / DMA */
// 后面的 ADMA 等高阶特性在裸机手搓阶段暂时不用
// ……

#define SD2_CONTROL2        (*(volatile unsigned int *)  (HSMMC2_BASE + 0x80))
#define SD2_CONTROL3        (*(volatile unsigned int *)  (HSMMC2_BASE + 0x84))
#define SD2_CONTROL4        (*(volatile unsigned int *)  (HSMMC2_BASE + 0x8C))

#define GPG2CON             (*(volatile unsigned int *)0xE02001E0)
#define GPG2PUD             (*(volatile unsigned int *)0xE02001E8)
#define GPG2DRV             (*(volatile unsigned int *)0xE02001EC)

/* SDHCI HOSTCTL bits */
#define SD_HOSTCTL_4BIT            (1U << 1)
#define SD_HOSTCTL_HIGHSPEED       (1U << 2)
/* Samsung S5PV210 CONTROL2 */
#define SD_CTRL2_ENSTAASYNCCLR     (1U << 31)
#define SD_CTRL2_ENCMDCNFMSK       (1U << 30)
#define SD_CTRL2_ENFBCLKTX         (1U << 15)
#define SD_CTRL2_ENFBCLKRX         (1U << 14)
#define SD_CTRL2_ENCLKOUTHOLD      (1U << 8)
#define SD_CTRL2_SCLK_MMC          (2U << 4)
/*
 * CONTROL3:
 * Samsung S5PV210 Linux 驱动在 > 400kHz 时：
 * TX = BASIC:
 *   FCSEL3(bit31) + FCSEL2(bit23)
 * 对普通 SD、非 Herring 特例：
 * RX = INVERT = 0
 */
#define SD_CTRL3_FCSELTX_BASIC     ((1U << 31) | (1U << 23))
#define SD_CTRL3_FCSELRX_INVERT    0U


// 所有的 SD 卡在上电时都是“耳聋”的 如果直接用几十兆赫兹的高频和它说话，它根本听不见
// 国际 SD 标准规定，握手阶段的总线时钟必须降低到 400kHz 以内
void hci_controller_init(void)
{
    unsigned int timeout;
    /*
     * 1. 配置 X210 的 SD2 GPIO
     * GPG2:
     *   GPG2_0 = SD2_CLK
     *   GPG2_1 = SD2_CMD
     *   GPG2_2 = SD2_CD
     *   GPG2_3 = SD2_D0
     *   GPG2_4 = SD2_D1
     *   GPG2_5 = SD2_D2
     *   GPG2_6 = SD2_D3
     * X210/S5PV210 的 HSMMC2 使用 Special Function 2
     */
    GPG2CON = 0x02222222;
    /*
     * 只给 CD 检测脚 GPG2_2 打开上拉
     * 不给 CLK/CMD/DATA0~3 强行打开 SoC 内部上拉
     * Samsung 的 S5PV210 SDHCI GPIO 配置也是这种思路
     */
    GPG2PUD = 0x00000020;
    // 设置 Memory Port 的驱动能力
    GPG2DRV = 0x00003FFF;
    /*
     * 2. Host Controller 全局软件复位
     * 注意：
     *  CONTROL2 / CONTROL3 / CONTROL4
     *  必须在这个 reset 完成以后再配置
     *
     * 之前把它们放在 SWRST 前面，
     * reset 会把这些配置冲掉
     */
    SD2_SWRST = 0x01;
    timeout = 100000;
    while (SD2_SWRST & 0x01) {
        if (--timeout == 0) {
            printk("HSMMC2 reset timeout!\r\n");
            while (1){}
        }
    }
    /*
     * 3. Host Controller 专用配置
     *
     * CONTROL2:
     *  bit31 = ENSTAASYNCCLR
     *  bit30 = ENCMDCNFMSK
     *  bit8  = ENCLKOUTHOLD
     *  bit5:4 = 2 -> SCLK_MMC2
     *
     * 这里必须在 SWRST 之后写
     */
    SD2_CONTROL2 = (1U << 31) |
                   (1U << 30) |
                   (1U << 8)  |
                   (2U << 4);
    /*
     * 4. 低速初始化阶段的 CONTROL3
     * 当前 SDCLK <= 400kHz，
     * 按 Samsung 的 S5PV210 参考配置：
     *   CONTROL3 = 0
     * 暂时不开启 feedback clock / timing adjustment
     */
    SD2_CONTROL3 = 0;
    // 5. CONTROL4
    SD2_CONTROL4 = (3U << 16);  // Samsung 驱动使用 9mA drive
    // 6. 清除 Host Controller 中已有的状态
    SD2_NORINTSTS = 0xFFFF;
    SD2_ERRINTSTS = 0xFFFF;
    // 开启状态寄存器
    SD2_NORINTSTSEN = 0xFFFF;
    SD2_ERRINTSTSEN = 0xFFFF;
    // 数据/命令超时配置
    SD2_TIMEOUTCON = 0x0E;
    // 7. 开启 HSMMC2 内部时钟
    SD2_CLKCON &= ~(1 << 0);
    SD2_CLKCON |=  (1 << 0);
    timeout = 100000;
    while (!(SD2_CLKCON & (1 << 1))) {
        if (--timeout == 0) {
            printk("HSMMC2 internal clock unstable!\r\n");
            while (1) {}
        }
    }
    /*
     * 8. 初始化阶段 SDCLK
     * 假定：
     *   SCLK_MMC2 = 48MHz
     * 0x80 = /256
     * 48MHz / 256 = 187.5kHz
     * <= 400kHz，适合 SD 卡初始化
     */
    SD2_CLKCON &= ~(0xFF << 8);
    SD2_CLKCON |=  (0x80 << 8);
    // 打开 SD clock
    SD2_CLKCON |= (1 << 2);
    // 9. 打开 SD 电源
    SD2_PWRCON = (0x07 << 1) | (1 << 0);
    /*
     * 10. 上电后给 SD 卡留一毫秒启动时间
     * 同时初始化阶段 SDCLK 很慢
     * 已经具备满足 SD 上电初始化时钟要求的条件
     */
    delay_ms(1);
    printk("HSMMC2 initialized: CTRL2=%x CTRL3=%x CTRL4=%x CLKCON=%x\r\n",
            SD2_CONTROL2, SD2_CONTROL3, SD2_CONTROL4, SD2_CLKCON);
}

void hci_set_bus_width_4bit() {
    SD2_HOSTCTL |= (1U << 1); // SD_HOSTCTL_4BIT
}

/*
 * 修改 HSMMC2 输出 SDCLK
 * divider:
 *   0x80 -> /256 -> 48MHz / 256 = 187.5kHz
 *   0x01 -> /2   -> 48MHz / 2   = 24MHz
 *   0x00 -> /1   -> 48MHz
 */
static int hci_host_set_clock(unsigned int divider)
{
    unsigned int timeout;
    // 先停止向卡输出 SDCLK
    SD2_CLKCON &= ~(1 << 2);

    // SD2_CLKCON &= ~(1 << 0);   // bit 0 是 SDHCI 控制器内部状态机的“心脏”，依据标准不应该被关闭
    SD2_CLKCON &= ~(0xFF << 8);
    SD2_CLKCON |= ((divider & 0xFF) << 8);

    // 确保内部时钟开启，并等待它稳定
    SD2_CLKCON |= (1 << 0);
    timeout = 100000;
    while (!(SD2_CLKCON & (1 << 1))) {
        if (--timeout == 0) {
            printk("[ERROR] SD internal clock unstable.\r\n");
            return -1;
        }
    }
    // internal clock stable 后，再输出给 SD 卡
    SD2_CLKCON |= (1 << 2);
    // 给 SD 卡的锁相环 (PLL) 10 毫秒的适应时间
    delay_ms(10);
    return 0;
}

/*
 * 配置 S5PV210 HSMMC 在 >400kHz 时使用的 feedback timing
 * 这里采用 Samsung S5PV210 Linux 驱动对普通 SD 的配置：
 *   TX feedback enabled
 *   RX feedback enabled
 *   TX = BASIC
 *   RX = INVERT
 * X210 不是 Samsung Herring 的特殊 WiMAX 情况，
 * 因此走普通 SD 分支即可
 */
static inline void hci_host_enable_fast_timing(void)
{
    SD2_CONTROL2 = SD_CTRL2_ENSTAASYNCCLR |
                   SD_CTRL2_ENCMDCNFMSK   |
                   SD_CTRL2_ENFBCLKTX     |
                   SD_CTRL2_ENFBCLKRX     |
                   SD_CTRL2_ENCLKOUTHOLD  |
                   SD_CTRL2_SCLK_MMC;

    SD2_CONTROL3 = SD_CTRL3_FCSELTX_BASIC |
                   SD_CTRL3_FCSELRX_INVERT;
}

int hci_set_default_speed() {
    // 关闭主板的高速模式位
    SD2_HOSTCTL &= ~(1U << 2); // SD_HOSTCTL_HIGHSPEED
    // 开启三星私有的反馈时钟
    hci_host_enable_fast_timing(); // 这个原有的内部函数保留在 sd_hci.c 即可
    // 设置时钟分频为 0x01 (48MHz / 2 = 24MHz)
    if (hci_host_set_clock(0x01) != 0) return -1;
    return 0;
}

int hci_set_high_speed() {
    // 开启主板的高速模式位
    SD2_HOSTCTL |= (1U << 2); // SD_HOSTCTL_HIGHSPEED
    hci_host_enable_fast_timing();
    // 设置时钟分频为 0x00 (48MHz 直通)
    if (hci_host_set_clock(0x00) != 0) return -1;
    return 0;
}

static inline int sd_wait_cmd_inhibit_clear(void)
{
    unsigned int timeout = 1000000;
    /*
     * PRNSTS:
     *  bit0 = CMD_INHIBIT
     *  bit1 = DAT_INHIBIT
     * 对数据命令来说，两者都应该清零
     */
    while (SD2_PRNSTS & 0x3) {
        if (--timeout == 0) {
            printk("[ERROR] CMD/DAT inhibit timeout! PRNSTS=%x\r\n", SD2_PRNSTS);
            return -1;
        }
    }
    return 0;
}

/* 当发生错误时，必须复位 CMD 线路(bit 1)和 DAT 线路(bit 2) */
static inline void sd_clear_error(void) {
    // 往 SWRST 写 1 复位，硬件复位完成后会自动清零
    SD2_SWRST |= (1 << 1) | (1 << 2);
    unsigned int timeout = 100000;
    while (SD2_SWRST & ((1 << 1) | (1 << 2))) {
        if (--timeout == 0) {
            printk("[WARN] SD Controller SWRST timeout!\r\n");
            break;
        }
    }
}

/* 发送命令的底层驱动引擎 */
// HSMMC2 的物理基址是 0xEB200000 SD 卡控制器的核心逻辑是：
//    往 ARGUMENT 寄存器填参数，往 CMDREG 发指令，然后死等 NORINTSTS（中断状态）里的“命令完成”标志
int hci_send_cmd(unsigned short cmd, unsigned int arg) {
    unsigned int timeout;
    // 等待 CMD 线路空闲
    if (sd_wait_cmd_inhibit_clear() < 0)
        return -1;
    // 2. 清除之前的所有中断状态 (写 1 清零)
    SD2_NORINTSTS = 0xFFFF;
    SD2_ERRINTSTS = 0xFFFF;
    // 3. 填入 32 位参数
    SD2_ARGUMENT = arg;
    // 4. 发射 16 位命令
    SD2_CMDREG = cmd;
    // 5. 等待命令发送完成 (NORINTSTS bit 0: Command Complete)
    timeout = 1000000;
    while (!(SD2_NORINTSTS & (1 << 0))) {
        unsigned short err = SD2_ERRINTSTS;
        if (err) {
            // CPU 现在跑在 1000MHz (1GHz)，而 SD 卡在握手阶段的总线频率被我们压到了 400kHz 以下
            // SD 卡在接收到上电指令后，内部的微控制器需要执行极其缓慢的固件初始化（通常需要几毫秒到几十毫秒）
            // CMD error: ERRINTSTS=0x8   // SD 卡繁忙， 控制器等不到回复的超时错误
            // CMD error: ERRINTSTS=0x1   // SD 卡电平乱了，控制器发现回复的命令号（Index）跟我们发出去的对不上
            // 所以硬件探测阶段（Probing）出现读写错误是完全预期内的行为，注释掉报错
            // printk("CMD error: ERRINTSTS=%x\r\n", err);
            SD2_ERRINTSTS = err;
            printk("\r\n[ERROR] Command failed: CMD=%x ARG=%x ERRINTSTS=%x PRNSTS=%x\r\n", cmd, arg, err, SD2_PRNSTS);
            sd_clear_error(); // 【重启死锁的状态机】
            return -1;
        }
        if (--timeout == 0) {
            printk("\r\n[ERROR] Command timeout: CMD=%x ARG=%x PRNSTS=%x\r\n", cmd, arg, SD2_PRNSTS);
            sd_clear_error(); // 【重启死锁的状态机】
            return -1;
        }
    }
    // 6. 清除完成标志
    SD2_NORINTSTS = (1 << 0);
    // unsigned int r1 = SD2_RSPREG0;
    // printk("\r\nCommand Complete\r\n"
    //     "  R1     = %x\r\n"
    //     "  PRNSTS = %x\r\n"
    //     "  NOR    = %x\r\n"
    //     "  ERR    = %x\r\n"
    //     "  CTRL2  = %x\r\n"
    //     "  CTRL3  = %x\r\n"
    //     "  CTRL4  = %x\r\n"
    //     "  CLKCON = %x\r\n",
    //     r1,
    //     SD2_PRNSTS,
    //     SD2_NORINTSTS,
    //     SD2_ERRINTSTS,
    //     SD2_CONTROL2,
    //     SD2_CONTROL3,
    //     SD2_CONTROL4,
    //     SD2_CLKCON
    // );
    return 0;
}

unsigned int hci_get_response_0() {
    return SD2_RSPREG0;
}


/*
 * 准备数据传输硬件环境
 * blksize: 块大小 (通常是 512)
 * blkcnt:  传输多少个块 (单块为 1，多块为 N)
 * is_read: 1 代表读，0 代表写
 */
void hci_prepare_data_transfer(unsigned short blksize, unsigned short blkcnt, int is_read) {
    SD2_BLKSIZE = blksize;
    SD2_BLKCNT  = blkcnt;
    // bit 1 = Block Count Enable (启用块计数)
    unsigned short trnmod = (1 << 1);
    // bit 4 = Data Transfer Direction (传输方向)
    if (is_read) {
        trnmod |= (1 << 4);
    }
    // 未来要读写多个块，提前把逻辑写好
    if (blkcnt > 1) {
        trnmod |= (1 << 5); // Multi Block Select (多块模式)
        trnmod |= (1 << 2); // Auto CMD12 Enable (多块传输完毕后自动发 CMD12 停止)
    }
    SD2_TRNMOD = trnmod;
}

// 等待 FIFO 就绪 (Buffer Read Ready: bit 5)
int hci_wait_buffer_read_ready() {
    int timeout = 10000000;
    while (!(SD2_NORINTSTS & (1 << 5))) {
      unsigned short err = SD2_ERRINTSTS;
      if (err != 0) {
        printk("\r\n[ERROR] Wait FIFO Failed!\r\n"
               "  ERRINTSTS = %x\r\n"
               "  PRNSTS    = %x\r\n"
               "  NORINTSTS = %x\r\n",
               err, SD2_PRNSTS, SD2_NORINTSTS);
        sd_clear_error();
        return -1;
      }
      if (--timeout == 0) {
        printk("\r\n[ERROR] Wait FIFO Timeout!\r\n"
               "  PRNSTS    = %x\r\n"
               "  NORINTSTS = %x\r\n",
               SD2_PRNSTS, SD2_NORINTSTS);
        sd_clear_error();
        return -1;
      }
    }
    SD2_NORINTSTS = (1 << 5); // 内部自己把硬件状态清掉
    return 0;
}

// 榨取 FIFO 数据
void hci_read_fifo_data(unsigned int *buffer, int words) {
    for (int i = 0; i < words; i++) {
        buffer[i] = SD2_BDATA;
    }
}

// 等待传输彻底完成 (Transfer Complete: bit 1)
int hci_wait_transfer_complete() {
    int timeout = 10000000;
    while (!(SD2_NORINTSTS & (1 << 1))) {
        unsigned short err = SD2_ERRINTSTS;
        if (SD2_ERRINTSTS || --timeout == 0)
        {
            printk("\r\n[ERROR] Transfer Complete Failed! ERRINTSTS=%x (timeout=%x)\r\n", err, timeout);
            sd_clear_error();
            return -1;
        }
    }
    SD2_NORINTSTS = (1 << 1);
    return 0;
}
