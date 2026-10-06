#include "sd.h"
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


#define SD2_CONTROL2        (*(volatile unsigned int *)  (HSMMC2_BASE + 0x80))
#define SD2_CONTROL3        (*(volatile unsigned int *)  (HSMMC2_BASE + 0x84))
#define SD2_CONTROL4        (*(volatile unsigned int *)  (HSMMC2_BASE + 0x8C))

#define GPG2CON             (*(volatile unsigned int *)0xE02001E0)
#define GPG2PUD             (*(volatile unsigned int *)0xE02001E8)
#define GPG2DRV             (*(volatile unsigned int *)0xE02001EC)


/* 6. 高级特性 / DMA */
// 后面的 ADMA 等高阶特性在裸机手搓阶段暂时不用
// ……

int is_sdhc = 0; // 0代表小容量卡(SDSC)，1代表大容量卡(SDHC)
/* 当发生错误时，必须复位 CMD 线路(bit 1)和 DAT 线路(bit 2) */
static void sd_clear_error(void) {
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


static int sd_wait_cmd_inhibit_clear(void)
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

/* 发送命令的底层驱动引擎 */
// HSMMC2 的物理基址是 0xEB200000 SD 卡控制器的核心逻辑是：
//    往 ARGUMENT 寄存器填参数，往 CMDREG 发指令，然后死等 NORINTSTS（中断状态）里的“命令完成”标志
static int sd_send_cmd(unsigned short cmd, unsigned int arg) {
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
            printk("\r\n[ERROR] Command failed: CMD=%x ARG=%x ERRINTSTS=%x PRNSTS=%x\r\n",
                  cmd, arg, err, SD2_PRNSTS);
            sd_clear_error(); // 【重启死锁的状态机】
            return -1;
        }
        if (--timeout == 0) {
            printk("CMD timeout: CMD=%x ARG=%x PRNSTS=%x\r\n",
                  cmd, arg, SD2_PRNSTS);
            printk("\r\n[ERROR] Command timeout: CMD=%x ARG=%x PRNSTS=%x\r\n",
                  cmd, arg, SD2_PRNSTS);
            sd_clear_error(); // 【重启死锁的状态机】
            return -1;
        }
    }
    // 6. 清除完成标志
    SD2_NORINTSTS = (1 << 0);
    return 0;
}

// 所有的 SD 卡在上电时都是“耳聋”的 如果直接用几十兆赫兹的高频和它说话，它根本听不见
// 国际 SD 标准规定，握手阶段的总线时钟必须降低到 400kHz 以内
void sd_controller_init(void)
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


int sd_card_handshake(void) {
    printk("Starting SD Card Handshake...\r\n");
    /*
     * 步骤 1：发送 CMD0 (GO_IDLE_STATE)
     * 作用：让 SD 卡进行软件复位，进入空闲状态
     * 响应：无响应 (No Response)
     */
    // CMD 0 (0x00<<8) | 无响应 (0x00) = 0x0000
    sd_send_cmd(0x0000, 0x00000000);
    printk("CMD0 sent. Card reset.\r\n");

    /*
     * 步骤 2：发送 CMD8 (SEND_IF_COND)
     * 作用：探明这到底是不是一张支持 SD2.0(SDHC/SDXC) 的新卡
     * 参数：0x000001AA (0x1表示2.7-3.6V，0xAA是测试检验码)
     * 响应：48位响应 (0x10) + 检查CRC (0x08) + 检查Index (0x10)
     */
    // CMD 8 (0x08<<8) | Index(0x10) | CRC(0x08) | Resp48(0x02) = 0x081A
    sd_send_cmd(0x081A, 0x000001AA);

    // 校验响应：如果卡正常，它会原样返回 0x1AA
    if ((SD2_RSPREG0 & 0xFFF) != 0x1AA) {
        printk("CMD8 Failed! Not a valid SD2.0 Card.\r\n");
        return -1;
    }
    printk("CMD8 Passed! SD2.0 Card detected.\r\n");

    /*
     * 步骤 3：发送 ACMD41 (SD_SEND_OP_COND) 循环询问
     * 作用：不断询问 SD 卡 是否就绪
     * 注意：ACMD 必须先发 CMD55 (APP_CMD) 打头阵
     */
    unsigned int response;
    int timeout = 10000;
    do {
        // 先发 CMD55 (0x37)
        // CMD 55 (0x37<<8) | Index(0x10) | CRC(0x08) | Resp48(0x02) = 0x371A
        if (sd_send_cmd(0x371A, 0x00000000) == 0) {
            // CMD55 成功了，再发 ACMD41 (0x29)
            // 参数 0x40300000: 告知卡我是高容量主机 (HCS=1)，电压 3.2-3.4V
            // CMD 41 (0x29<<8) | Resp48(0x02) (ACMD41不检CRC和Index) = 0x2902
            if (sd_send_cmd(0x2902, 0x40300000) == 0) {
                // 只有两条命令都成功，才去读取响应
                // 只要响应的最高位 (bit 31) 为 1，说明卡已经上电初始化完成 (Power up status)
                response = SD2_RSPREG0;
                if (response & (1 << 31)) {
                    // 检查第 30 位 (CCS)，判断是否为高容量卡
                    if (response & (1 << 30)) {
                        is_sdhc = 1;
                        printk("Card is SDHC/SDXC (Block Addressing)\r\n");
                    } else {
                        is_sdhc = 0;
                        printk("Card is SDSC (Byte Addressing)\r\n");
                    }
                    break; // 卡片 Ready，成功跳出循环！
                }
            }
        }
        timeout--;
        // 等待10毫秒，给 SD 卡一点内部初始化的喘息时间，避免总线拥堵
        delay_ms(10);
    } while (timeout > 0);

    if (timeout == 0) {
        printk("ACMD41 Timeout! Card is dead.\r\n");
        return -1;
    }

    printk("ACMD41 Passed! SD Card is Ready for Action.\r\n");
    return 0;
}

// SD 卡设计之初是为了支持一根总线上挂多张卡的，所以必须先识别身份，并分配相对地址（Relative Card Address, RCA）
unsigned int rca = 0; // 全局变量，保存卡的“工号”
int sd_card_identify(void) {
    printk("Identifying Card...\r\n");

    /*
     * 1. 发送 CMD2 (ALL_SEND_CID)
     * 作用：让总线上的卡广播自己的 128 位身份信息 (厂商、生产日期等)
     * 响应：136位响应 (0x01) + 检查CRC (0x08) = 0x09
     */
    // CMD 2 (0x02<<8) | 0x09 = 0x0209
    sd_send_cmd(0x0209, 0x00000000);
    // 此时 SD2_RSPREG0 ~ SD2_RSPREG3 里装满了 CID 信息，我们这里不需要解析，直接跳过
    printk("CMD2 Passed.\r\n");

    /*
     * 2. 发送 CMD3 (SEND_RELATIVE_ADDR)
     * 作用：要求卡片发布自己的 RCA (相对地址)，这是以后和它单线联系的唯一凭证！
     * 响应：48位响应 (0x10) + 检查CRC (0x08) + 检查Index (0x10) = 0x1A
     */
    sd_send_cmd(0x031A, 0x00000000);

    // 响应寄存器的高 16 位就是卡片分配给自己的 RCA
    rca = (SD2_RSPREG0 >> 16) & 0xFFFF;
    printk("CMD3 Passed. Card RCA is: %x\r\n", rca);

    /*
     * 3. 发送 CMD7 (SELECT_CARD)
     * 作用：带着刚才拿到的 RCA 呼叫这张卡，让它进入 Transfer (传输) 状态
     */
    // 参数是 RCA 左移 16 位
    sd_send_cmd(0x071A, (rca << 16));
    delay_ms(20);
    printk("CMD7 Passed. Card selected and in Transfer State!\r\n");

    return 0;
}

// 默认情况下，SD 卡是用 1 根数据线（1-bit）在慢速（400kHz）下通信的 为了接下来的正常使用，我们必须把总线拓宽到 4 线，并把时钟拉高
void sd_set_high_speed(void) {
    /*
     * 1. 发送 ACMD6 (SET_BUS_WIDTH) 切换到 4 线模式
     * 注意：ACMD 必须先发 CMD55 (带 RCA)
     */
    sd_send_cmd(0x371A, (rca << 16)); // CMD55
    // 参数: 0b10 (2) 表示 4-bit bus
    sd_send_cmd(0x061A, 0x00000002);  // ACMD6

    // 让 S5PV210 控制器也切换到 4 线模式 (HOSTCTL2 bit 1)
    SD2_HOSTCTL &= ~(1 << 1);
    SD2_HOSTCTL |=  (1 << 1);
    printk("4-bit Bus Mode Enabled.\r\n");

    /*
     * 2. 提高物理时钟 (比如切到 24MHz 或 48MHz)
     * 我们关闭内部时钟，修改分频，然后再开启
     */
    SD2_CLKCON &= ~(1 << 2); // 关 SD 时钟输出
    SD2_CLKCON &= ~(1 << 0); // 关 内部时钟

    // 将分频器改小 (具体数值取决于输入时钟 PCLK)
    // 这里以 2 分频为例 (极大地提高传输速度)
    SD2_CLKCON &= ~(0xFF << 8);
    SD2_CLKCON |= (0x02 << 8);

    SD2_CLKCON |= (1 << 0);
    while (!(SD2_CLKCON & (1 << 1))); // 等稳定
    SD2_CLKCON |= (1 << 2);

    printk("High Speed Clock Enabled.\r\n");
}

int init_sd()
{
  sd_controller_init();
  if (sd_card_handshake() == 0) {
      sd_card_identify();
      // sd_set_high_speed();
      return 0;
  }
  return -1;
}

/* 读取一个 512 字节的扇区到内存 */
int sd_read_single_block(unsigned int sector, unsigned char *buffer) {
    unsigned int timeout;

    SD2_BLKSIZE = 512;
    SD2_BLKCNT = 1;
    SD2_TRNMOD = (1 << 4) | (1 << 1);

    // 判断是小卡还是大卡
    unsigned int arg = is_sdhc ? sector : (sector * 512);
    if (sd_send_cmd(0x113A, arg) != 0) {
      printk("[ERROR] CMD17 command phase failed. ERR=%x\r\n", SD2_ERRINTSTS);
      return -1;
    }

    // unsigned int r1 = SD2_RSPREG0;
    // printk(
    //     "\r\n"
    //     "[CMD17] Command Complete\r\n"
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

    // 等待 FIFO 就绪 (Buffer Read Ready: bit 5)
    timeout = 10000000;
    while (!(SD2_NORINTSTS & (1 << 5))) {
      unsigned short err = SD2_ERRINTSTS;
      if (err != 0) {
        printk(
            "\r\n"
            "[ERROR] Wait FIFO Failed!\r\n"
            "  ERRINTSTS = %x\r\n"
            "  PRNSTS    = %x\r\n"
            "  NORINTSTS = %x\r\n",
            err,
            SD2_PRNSTS,
            SD2_NORINTSTS
        );
        sd_clear_error();
        return -1;
      }
      if (--timeout == 0) {
        printk(
            "\r\n"
            "[ERROR] Wait FIFO Timeout!\r\n"
            "  PRNSTS    = %x\r\n"
            "  NORINTSTS = %x\r\n",
            SD2_PRNSTS,
            SD2_NORINTSTS
        );
        sd_clear_error();
        return -1;
      }
    }
    SD2_NORINTSTS = (1 << 5);

    // 榨取 FIFO 数据
    unsigned int *ptr = (unsigned int *)buffer;
    for (int i = 0; i < 128; i++) {
        ptr[i] = SD2_BDATA;
    }

    // 等待传输彻底完成 (Transfer Complete: bit 1)
    timeout = 10000000;
    while (!(SD2_NORINTSTS & (1 << 1))) {
        unsigned short err = SD2_ERRINTSTS;
        if (SD2_ERRINTSTS || (--timeout == 0)) {
            printk("\r\n[ERROR] Transfer Complete Failed! ERRINTSTS=%x (timeout=%x)\r\n", err, timeout);
            sd_clear_error();
            return -1;
        }
    }
    SD2_NORINTSTS = (1 << 1);
    return 0;
}
