#include "ddr.h"
#include "../02_uart/uart.h"
#include "../05_timer/timer.h"

/*
 * GPIO / Memory Port 1
 * MP1 是 DMC0 所连接的 DDR2 数据/地址/控制信号对应的
 * Memory Port。
 *
 * 每组 MP1_x 的寄存器排列为：
 *   +0x00  CON
 *   +0x04  DAT
 *   +0x08  PUD
 *   +0x0C  DRV
 * 所以真正的 DRV 地址如下。
 * 这里设置为 2X drive strength。
 * 公开的 S5PV210 DDR2 初始化代码也通常采用：
 *   MP1_0 ~ MP1_7 = 0xAAAA
 *   MP1_8         = 0x2AAA
 * 其中 0b10 对应 2X drive。
 */
#define MP1_0DRV_SR1    (*(volatile unsigned int *)0xE02003CC)
#define MP1_1DRV_SR1    (*(volatile unsigned int *)0xE02003EC)
#define MP1_2DRV_SR1    (*(volatile unsigned int *)0xE020040C)
#define MP1_3DRV_SR1    (*(volatile unsigned int *)0xE020042C)
#define MP1_4DRV_SR1    (*(volatile unsigned int *)0xE020044C)
#define MP1_5DRV_SR1    (*(volatile unsigned int *)0xE020046C)
#define MP1_6DRV_SR1    (*(volatile unsigned int *)0xE020048C)
#define MP1_7DRV_SR1    (*(volatile unsigned int *)0xE02004AC)
#define MP1_8DRV_SR1    (*(volatile unsigned int *)0xE02004CC)

// DMC0
#define DMC0_BASE       0xF0000000
// DMC Control registers
#define DMC0_CONCONTROL     (*(volatile unsigned int *)(DMC0_BASE + 0x00))
#define DMC0_MEMCONTROL     (*(volatile unsigned int *)(DMC0_BASE + 0x04))
#define DMC0_MEMCONFIG0     (*(volatile unsigned int *)(DMC0_BASE + 0x08))
#define DMC0_MEMCONFIG1     (*(volatile unsigned int *)(DMC0_BASE + 0x0C))
#define DMC0_DIRECTCMD      (*(volatile unsigned int *)(DMC0_BASE + 0x10))
#define DMC0_PRECHCONFIG    (*(volatile unsigned int *)(DMC0_BASE + 0x14))
// PHY
#define DMC0_PHYCONTROL0   (*(volatile unsigned int *)(DMC0_BASE + 0x18))
#define DMC0_PHYCONTROL1   (*(volatile unsigned int *)(DMC0_BASE + 0x1C))
#define DMC0_PHYSTATUS     (*(volatile unsigned int *)(DMC0_BASE + 0x40)) // PHY status
// Power Down
#define DMC0_PWRDNCONFIG   (*(volatile unsigned int *)(DMC0_BASE + 0x28))
#define DMC0_TIMINGAREF    (*(volatile unsigned int *)(DMC0_BASE + 0x30))
#define DMC0_TIMINGROW     (*(volatile unsigned int *)(DMC0_BASE + 0x34))
#define DMC0_TIMINGDATA    (*(volatile unsigned int *)(DMC0_BASE + 0x38))
#define DMC0_TIMINGPOWER   (*(volatile unsigned int *)(DMC0_BASE + 0x3C))


// Memory Port 2 (负责 DMC1 的物理引脚驱动能力)
#define MP2_0DRV_SR1    (*(volatile unsigned int *)0xE02004EC)
#define MP2_1DRV_SR1    (*(volatile unsigned int *)0xE020050C)
#define MP2_2DRV_SR1    (*(volatile unsigned int *)0xE020052C)
#define MP2_3DRV_SR1    (*(volatile unsigned int *)0xE020054C)
#define MP2_4DRV_SR1    (*(volatile unsigned int *)0xE020056C)
#define MP2_5DRV_SR1    (*(volatile unsigned int *)0xE020058C)
#define MP2_6DRV_SR1    (*(volatile unsigned int *)0xE02005AC)
#define MP2_7DRV_SR1    (*(volatile unsigned int *)0xE02005CC)
#define MP2_8DRV_SR1    (*(volatile unsigned int *)0xE02005EC)
// DMC1 (DRAM Controller 1) 寄存器基地址
#define DMC1_BASE       0xF1400000

#define DMC1_CONCONTROL     (*(volatile unsigned int *)(DMC1_BASE + 0x00))
#define DMC1_MEMCONTROL     (*(volatile unsigned int *)(DMC1_BASE + 0x04))
#define DMC1_MEMCONFIG0     (*(volatile unsigned int *)(DMC1_BASE + 0x08))
#define DMC1_MEMCONFIG1     (*(volatile unsigned int *)(DMC1_BASE + 0x0C))
#define DMC1_DIRECTCMD      (*(volatile unsigned int *)(DMC1_BASE + 0x10))
#define DMC1_PRECHCONFIG    (*(volatile unsigned int *)(DMC1_BASE + 0x14))

#define DMC1_PHYCONTROL0   (*(volatile unsigned int *)(DMC1_BASE + 0x18))
#define DMC1_PHYCONTROL1   (*(volatile unsigned int *)(DMC1_BASE + 0x1C))
#define DMC1_PHYSTATUS     (*(volatile unsigned int *)(DMC1_BASE + 0x40))

#define DMC1_PWRDNCONFIG   (*(volatile unsigned int *)(DMC1_BASE + 0x28))
#define DMC1_TIMINGAREF    (*(volatile unsigned int *)(DMC1_BASE + 0x30))
#define DMC1_TIMINGROW     (*(volatile unsigned int *)(DMC1_BASE + 0x34))
#define DMC1_TIMINGDATA    (*(volatile unsigned int *)(DMC1_BASE + 0x38))
#define DMC1_TIMINGPOWER   (*(volatile unsigned int *)(DMC1_BASE + 0x3C))


/*
 * 等待 PHY DLL LOCK
 * PHYSTATUS[2:0]：
 *   0x7 = DLL locked / PHY ready
 * 这里不能简单依靠：
 *   delay(0x10000)
 * 因为 PLL / DLL 是否锁定与芯片、时钟以及环境有关。
 * 直接检查状态更加可靠。
 */

static int ddr_wait_phy_lock(int num)
{
    unsigned int timeout = 1000000;
    while (timeout--) {
        if(num == 0)
        {
          if ((DMC0_PHYSTATUS & 0x7) == 0x7) {
              return 0;
          }
        }
        else if(num == 1)
        {
          if ((DMC1_PHYSTATUS & 0x7) == 0x7) {
              return 0;
          }
        }
        else return 0;
    }
    // DLL 没有成功 lock
    return -1;
}

/*
 * DDR2 Mode Register 初始化
 * 下面这一串 DIRECTCMD 是 DDR2 JEDEC 初始化 sequence。
 * cmd_type / cmd_bank / cmd_address 都被编码进 DIRECTCMD。
 *
 * 典型流程：
 *   NOP
 *   PALL
 *   EMRS2
 *   EMRS3
 *   EMRS1
 *   MRS + DLL reset
 *   PALL
 *   REFRESH
 *   REFRESH
 *   MRS normal
 *   EMRS OCD default
 *   EMRS OCD exit
 * ============================================================
 */

static void ddr2_mode_register_init(int num)
{
  if(num == 0)
  {
    // NOP 让 CKE 进入有效状态
    DMC0_DIRECTCMD = 0x07000000;
    // 上电后要求留出稳定时间
    delay_ms(10);
    // PALL Precharge All
    DMC0_DIRECTCMD = 0x01000000;
    // EMRS2
    DMC0_DIRECTCMD = 0x00020000;
    // EMRS3
    DMC0_DIRECTCMD = 0x00030000;
    /*
     * EMRS1
     * DDR2 DLL enable
     * DQS# 正常使用
     */
    DMC0_DIRECTCMD = 0x00010400;
    /*
     * MRS
     * DLL Reset
     * 0x542：
     *   BL = 4
     *   CAS Latency = 4
     *   DLL Reset = 1
     */
    DMC0_DIRECTCMD = 0x00000542;
    // PALL
    DMC0_DIRECTCMD = 0x01000000;
    // Auto Refresh
    DMC0_DIRECTCMD = 0x05000000;
    DMC0_DIRECTCMD = 0x05000000;
    // MRS 正常工作模式，不再 reset DLL。
    DMC0_DIRECTCMD = 0x00000442;
    // 等待至少若干个时钟周期，这里给一个保守延时
    delay_ms(10);
    /*
     * EMRS1
     * OCD default calibration
     */
    DMC0_DIRECTCMD = 0x00010780;
    /*
     * EMRS1
     * Exit OCD calibration。
     */
    DMC0_DIRECTCMD = 0x00010400;
  }
  else if(num == 1)
  {
    DMC1_DIRECTCMD = 0x07000000;
    delay_ms(10);
    DMC1_DIRECTCMD = 0x01000000;
    DMC1_DIRECTCMD = 0x00020000;
    DMC1_DIRECTCMD = 0x00030000;
    DMC1_DIRECTCMD = 0x00010400;
    DMC1_DIRECTCMD = 0x00000542;
    DMC1_DIRECTCMD = 0x01000000;
    DMC1_DIRECTCMD = 0x05000000;
    DMC1_DIRECTCMD = 0x05000000;
    DMC1_DIRECTCMD = 0x00000442;
    delay_ms(10);
    DMC1_DIRECTCMD = 0x00010780;
    DMC1_DIRECTCMD = 0x00010400;
  }
  else {}
}

void ddr_init(void)
{
    uart2_puts("\r\n");
    uart2_puts("========================================\r\n");
    uart2_puts("DDR2 initialization start...\r\n");
    uart2_puts("========================================\r\n");
    /*
     * 1. Memory Port 1 Drive Strength
     * Study210/X210 的 DMC0 DDR 信号由 MP1 这一组端口承担。
     * 这里使用 2X drive。
     */

    MP1_0DRV_SR1 = 0x0000AAAA;
    MP1_1DRV_SR1 = 0x0000AAAA;
    MP1_2DRV_SR1 = 0x0000AAAA;
    MP1_3DRV_SR1 = 0x0000AAAA;
    MP1_4DRV_SR1 = 0x0000AAAA;
    MP1_5DRV_SR1 = 0x0000AAAA;
    MP1_6DRV_SR1 = 0x0000AAAA;
    MP1_7DRV_SR1 = 0x0000AAAA;

    /*
     * MP1_8 有效字段较少，
     * 因此使用 0x2AAA。
     */
    MP1_8DRV_SR1 = 0x00002AAA;


    /*
     * 2. PHY DLL 基础配置
     * Study210 当前的 clock_init()
     * 设置 CPU = 1GHz。
     *
     * 这里采用 S5PV210 DDR2 200MHz 的常见参考配置：
     * PHYCONTROL0:
     *   start_point = 0x10
     *   ctrl_inc    = 0x10
     * PHYCONTROL1:
     *   ctrl_shiftc / DQS cleaning
     * 公开 S5PV210 DDR2 初始化资料给出的常用值：
     *   0x00101000
     *   0x00000086
     */
    DMC0_PHYCONTROL0 = 0x00101000;
    DMC0_PHYCONTROL1 = 0x00000086;

    // 3. 打开 DLL
    DMC0_PHYCONTROL0 = 0x00101002;

    /*
     * 4. 启动 PHY
     * ctrl_start = bit0
     */
    DMC0_PHYCONTROL0 = 0x00101003;

    // 5. 等待 DLL LOCK
    if (ddr_wait_phy_lock(0) != 0) {
        uart2_puts("ERROR: DDR PHY DLL lock timeout!\r\n");
        /*
         * 这里不能继续执行。
         * 如果 DLL 都没 lock，
         * 后面的内存初始化没有意义。
         */
        while (1) {
            /* 停在这里方便观察串口 */
        }
    }
    uart2_puts("DDR PHY DLL locked.\r\n");

    /*
     * 6. ConControl
     * 这一阶段必须保证 auto refresh counter 关闭。
     * 0x0FFF2010 是 S5PV210 DDR2 裸机初始化中非常常见的参考值。
     */
    DMC0_CONCONTROL = 0x0FFF2010;

    /*
     * 7. MemControl
     * DDR2
     * Burst Length = 4
     * 关闭各种 power-down
     * 公开资料常见值：0x00202400
     */
    DMC0_MEMCONTROL = 0x00202400;

    /*
     * 8. Memory Mapping
     * DMC0 映射到：
     *   0x20000000 ~ 0x2FFFFFFF
     * 共 256MB。
     *
     * 这是朱有鹏课程体系中常见的裸机映射方式。
     * 0x20 代表 chip base。
     * 下面使用：
     *   0x20F01323
     * 作为传统 S5PV210 DDR2 256MB 配置参考值。
     */
    DMC0_MEMCONFIG0 = 0x20F01323;
    // 9. PRECHARGE CONFIG
    DMC0_PRECHCONFIG = 0xFF000000;

    /*
     * 10. Power-down configuration
     * DDR 初始化/调试阶段尽量不要让 power-down
     * 介入，以减少变量。
     */
    DMC0_PWRDNCONFIG = 0xFFFF00FF;

    /*
     * 11. DDR Timing
     * 这里假定 DMC DDR clock = 200MHz。
     * tREFI:
     *   7.8us × 200MHz = 1560
     *                  = 0x618
     * 这四个寄存器的地址必须特别注意：
     *   TIMINGAREF  = +0x30
     *   TIMINGROW   = +0x34
     *   TIMINGDATA  = +0x38
     *   TIMINGPOWER = +0x3C
     * 以下数值采用 S5PV210 DDR2 常见 200MHz 参考参数。
     */
    DMC0_TIMINGAREF  = 0x00000618;
    DMC0_TIMINGROW   = 0x28233287;
    DMC0_TIMINGDATA  = 0x23240304;
    DMC0_TIMINGPOWER = 0x09C80232;

    // 12. DDR2 Mode Register 初始化
    ddr2_mode_register_init(0);


    /*
     * 13. 打开 Auto Refresh
     * 注意：
     *  DDR2 初始化完成后，
     *  才让 controller 接管 auto refresh。
     */

    DMC0_CONCONTROL = 0x0FF02030;
    // Power-down 继续保持关闭。
    DMC0_PWRDNCONFIG = 0xFFFF00FF;
    // MemControl 保持 DDR2 配置。
    DMC0_MEMCONTROL = 0x00202400;

    /* ==========================================
     * 第二阶段：初始化 DMC1 (克隆 DMC0 的时序，换个基址)
     * ========================================== */
    MP2_0DRV_SR1 = 0x0000AAAA;
    MP2_1DRV_SR1 = 0x0000AAAA;
    MP2_2DRV_SR1 = 0x0000AAAA;
    MP2_3DRV_SR1 = 0x0000AAAA;
    MP2_4DRV_SR1 = 0x0000AAAA;
    MP2_5DRV_SR1 = 0x0000AAAA;
    MP2_6DRV_SR1 = 0x0000AAAA;
    MP2_7DRV_SR1 = 0x0000AAAA;
    MP2_8DRV_SR1 = 0x00002AAA;

    DMC1_PHYCONTROL0 = 0x00101000;
    DMC1_PHYCONTROL1 = 0x00000086;
    DMC1_PHYCONTROL0 = 0x00101002;
    DMC1_PHYCONTROL0 = 0x00101003;

    if (ddr_wait_phy_lock(1) != 0) {
        uart2_puts("ERROR: DMC1 PHY DLL lock timeout!\r\n");
        while(1);
    }
    uart2_puts("DMC1 PHY DLL locked.\r\n");

    DMC1_CONCONTROL = 0x0FFF2010;
    DMC1_MEMCONTROL = 0x00202400;
    // DMC1 的 CS0 必须映射到 0x4000_0000
    // 0x40 代表 chip base
    DMC1_MEMCONFIG0 = 0x40F01323;
    DMC1_PRECHCONFIG = 0xFF000000;
    DMC1_PWRDNCONFIG = 0xFFFF00FF;

    DMC1_TIMINGAREF  = 0x00000618;
    DMC1_TIMINGROW   = 0x28233287;
    DMC1_TIMINGDATA  = 0x23240304;
    DMC1_TIMINGPOWER = 0x09C80232;

    ddr2_mode_register_init(1);

    DMC1_CONCONTROL = 0x0FF02030;
    DMC1_PWRDNCONFIG = 0xFFFF00FF;
    DMC1_MEMCONTROL = 0x00202400;

    uart2_puts("DDR2 DMC0 + DMC1 (512MB) fully operational.\r\n");
    uart2_puts("========================================\r\n");
}
