#include "clock.h"

#define CLK_BASE        0xE0100000
/* PLL Lock */
#define APLL_LOCK       (*(volatile unsigned int *)(CLK_BASE + 0x0000))
#define MPLL_LOCK       (*(volatile unsigned int *)(CLK_BASE + 0x0008))
#define EPLL_LOCK       (*(volatile unsigned int *)(CLK_BASE + 0x0010))
/* PLL Control */
#define APLL_CON0       (*(volatile unsigned int *)(CLK_BASE + 0x0100))
#define MPLL_CON        (*(volatile unsigned int *)(CLK_BASE + 0x0108))
#define EPLL_CON0       (*(volatile unsigned int *)(CLK_BASE + 0x0110))
/* Clock source */
#define CLK_SRC0        (*(volatile unsigned int *)(CLK_BASE + 0x0200))
#define CLK_SRC4        (*(volatile unsigned int *)(CLK_BASE + 0x0210))
/* Clock source mask */
#define CLK_SRC_MASK0   (*(volatile unsigned int *)(CLK_BASE + 0x0280))
/* Clock divider */
#define CLK_DIV0        (*(volatile unsigned int *)(CLK_BASE + 0x0300))
#define CLK_DIV4        (*(volatile unsigned int *)(CLK_BASE + 0x0310))
/* IP clock gate */
#define CLK_GATE_IP2    (*(volatile unsigned int *)(CLK_BASE + 0x0468))


void clock_init(void) {
    /* 设置 Lock Time */
    APLL_LOCK = 0x0000FFFF;
    MPLL_LOCK = 0x0000FFFF;
    EPLL_LOCK = 0x0000FFFF;

    /*
     * 启动 PLL (配置 M, P, S 参数)
     * APLL 公式: FOUT = (MDIV * 24MHz) / (PDIV * 2^(SDIV-1))
     * MPLL 公式: FOUT = (MDIV * 24MHz) / (PDIV * 2^SDIV)
     */
    APLL_CON0 = (1   << 31) | // ENABLE: 启动 APLL
                (125 << 16) | // MDIV
                (3   <<  8) | // PDIV
                (1   <<  0);  // SDIV  -> 结果为 1000MHz

    MPLL_CON  = (1   << 31) | // ENABLE: 启动 MPLL
                (667 << 16) | // MDIV
                (12  <<  8) | // PDIV
                (1   <<  0);  // SDIV  -> 结果为 667MHz

    /*
     * EPLL = 96MHz
     * 24MHz * 48 / (3 * 4) = 96MHz
     * 这条时钟非常重要：
     *  X210 的 HSMMC2 可以从 SCLK_EPLL 获取 clock。
     */
    EPLL_CON0 = (1  << 31) |
                (48 << 16) |
                (3  <<  8) |
                (2  <<  0);

    /*
     * 等待 PLL Lock
     * S5PV210 PLL CON0 的 bit29 是 lock 状态。
     */
    while (!(APLL_CON0 & (1 << 29)));
    while (!(MPLL_CON  & (1 << 29)));
    while (!(EPLL_CON0 & (1 << 29)));

    /* 配置系统时钟分频
     * 分频公式: RATIO = (输入源频率 / 目标输出频率) - 1
     * 目标：
     *    ARMCLK=1G        CPU 核心使用
     *    HCLK_MSYS=200M   内存控制器 (DRAM Controller)、内部 96KB SRAM 和直接内存访问控制器 (DMA)
     *    PCLK_MSYS=100M   给 MSYS 域内的安全模块 (TrustZone) 和一些底层状态控制寄存器供电
     *    HCLK_DSYS=166M   专门给吃显存带宽的“大户”供电，包括 FIMD（LCD 显示控制器）、MFC（视频硬件编解码器）和 3D GPU（PowerVR SGX540）。图像数据就是在 166MHz 的通道里。
     *    PCLK_DSYS=83M    给上述多媒体控制器的状态配置寄存器接口供电
     *    HCLK_PSYS=133M   给需要一定吞吐量的中速外部接口供电，比如 USB 主机/设备控制器、SD/MMC 卡控制器、以太网控制器
     *    PCLK_PSYS=66M    给最传统的慢速外设供电。UART、GPIO、PWM 蜂鸣器、看门狗统统挂载在这条总线上
     */
    CLK_DIV0 = (1 << 28) | // PCLK_PSYS_RATIO: 133M / (1+1) = 66M   (UART, PWM定时器)
               (4 << 24) | // HCLK_PSYS_RATIO: 667M / (4+1) = 133M  (USB, SD卡)
               (1 << 20) | // PCLK_DSYS_RATIO: 166M / (1+1) = 83M   (显示外设控制)
               (3 << 16) | // HCLK_DSYS_RATIO: 667M / (3+1) = 166M  (LCD, GPU显存通道)
               (1 << 12) | // PCLK_MSYS_RATIO: 200M / (1+1) = 100M  (主干道控制)
               (4 <<  8) | // HCLK_MSYS_RATIO: 1000M/ (4+1) = 200M  (DDR内存控制器)
               (4 <<  4) | // A2M_RATIO:       1000M/ (4+1) = 200M  (AXI高速总线)
               (0 <<  0);  // APLL_RATIO:      1000M/ (0+1) = 1000M (ARM CPU核心吃满)

    /*
     * 把 MOUT_APLL / MOUT_MPLL / MOUT_EPLL 切换到真正的 PLL 输出
     * bit0 = APLL_SEL
     * bit4 = MPLL_SEL
     * bit8 = EPLL_SEL
     * 1 = PLL
     * 0 = 外部晶振
     */
    CLK_SRC0 = (1 << 8) |
               (1 << 4) |
               (1 << 0);

    /*
     * 配置 HSMMC2 专用时钟
     * CLK_SRC4[11:8] = MMC2_SEL
     *   0 = XXTI
     *   1 = XUSBXTI
     *   ...
     *   6 = MPLL
     *   7 = EPLL
     * 选择：
     *   SCLK_MMC2 = EPLL = 96MHz
     */
    CLK_SRC4 &= ~(0xF << 8);
    CLK_SRC4 |= (0x7 << 8);


    /*
     * HSMMC2 时钟分频
     * MMC2_RATIO = 1
     * SCLK_MMC2 = 96MHz / (1 + 1)
     *            = 48MHz
     */
    CLK_DIV4 &= ~(0xF << 8);
    CLK_DIV4 |= (0x1 << 8);


    /*
     * 打开 SCLK_MMC2 的 source mask
     * Linux 的 S5PV210 clock driver 把：
     *   SCLK_MMC0 -> MASK0 bit8
     *   SCLK_MMC1 -> MASK0 bit9
     *   SCLK_MMC2 -> MASK0 bit10
     *   SCLK_MMC3 -> MASK0 bit11
     * enable 时 s5p_gatectrl() 会把对应 bit 置 1。
     */
    CLK_SRC_MASK0 |= (1 << 10);

    /*
     * 打开 HSMMC2 IP clock gate
     * CLK_GATE_IP2 bit18 = HSMMC2
     */
    CLK_GATE_IP2 |= (1 << 18);
    // 小延时，给时钟树一点稳定时间
    int tmp = 10000;
    while(--tmp);
}
