#include "clock.h"

#define APLL_LOCK (*(volatile unsigned int *)0xE0100000)
#define MPLL_LOCK (*(volatile unsigned int *)0xE0100008)

#define CLK_DIV0  (*(volatile unsigned int *)0xE0100300)

#define APLL_CON0 (*(volatile unsigned int *)0xE0100100)
#define MPLL_CON  (*(volatile unsigned int *)0xE0100108)

#define CLK_SRC0  (*(volatile unsigned int *)0xE0100200)

void clock_init(void) {
    /* 1. 设置 Lock Time */
    APLL_LOCK = 0x0000FFFF;
    MPLL_LOCK = 0x0000FFFF;

    /* 目标：
     * ARMCLK=1G        CPU 核心使用
     * HCLK_MSYS=200M   内存控制器 (DRAM Controller)、内部 96KB SRAM 和直接内存访问控制器 (DMA)
     * PCLK_MSYS=100M   给 MSYS 域内的安全模块 (TrustZone) 和一些底层状态控制寄存器供电
     * HCLK_DSYS=166M   专门给吃显存带宽的“大户”供电，包括 FIMD（LCD 显示控制器）、MFC（视频硬件编解码器）和 3D GPU（PowerVR SGX540）。图像数据就是在 166MHz 的通道里。
     * PCLK_DSYS=83M    给上述多媒体控制器的状态配置寄存器接口供电
     * HCLK_PSYS=133M   给需要一定吞吐量的中速外部接口供电，比如 USB 主机/设备控制器、SD/MMC 卡控制器、以太网控制器
     * PCLK_PSYS=66M    给最传统的慢速外设供电。UART、GPIO、PWM 蜂鸣器、看门狗统统挂载在这条总线上
     */
    /* 2. 配置分频系数 (必须在切时钟源前配置)
     * 分频公式: RATIO = (输入源频率 / 目标输出频率) - 1
     */
    CLK_DIV0 = (1 << 28) | // PCLK_PSYS_RATIO: 133M / (1+1) = 66M   (UART, PWM定时器)
               (4 << 24) | // HCLK_PSYS_RATIO: 667M / (4+1) = 133M  (USB, SD卡)
               (1 << 20) | // PCLK_DSYS_RATIO: 166M / (1+1) = 83M   (显示外设控制)
               (3 << 16) | // HCLK_DSYS_RATIO: 667M / (3+1) = 166M  (LCD, GPU显存通道)
               (1 << 12) | // PCLK_MSYS_RATIO: 200M / (1+1) = 100M  (主干道控制)
               (4 <<  8) | // HCLK_MSYS_RATIO: 1000M/ (4+1) = 200M  (DDR内存控制器)
               (4 <<  4) | // A2M_RATIO:       1000M/ (4+1) = 200M  (AXI高速总线)
               (0 <<  0);  // APLL_RATIO:      1000M/ (0+1) = 1000M (ARM CPU核心吃满)

    /* 3. 启动 PLL (配置 M, P, S 参数)
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

    /* 4. 切换时钟源：将系统时钟切到 PLL 上 */
    CLK_SRC0 = (1 << 4) | // MPLL_SEL: 0=晶振FIN, 1=使用刚刚锁定的 FOUTMPLL
               (1 << 0);  // APLL_SEL: 0=晶振FIN, 1=使用刚刚锁定的 FOUTAPLL
