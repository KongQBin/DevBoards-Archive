#pragma once

/* =========================================================================
 * 1. 硬件初始化与总线配置 (Hardware Init & Bus Configuration)
 * 负责主板 SDHCI 控制器的全局复位、电气属性配置、时钟频率切换与线宽设置。
 * ========================================================================= */
void hci_controller_init(void);
/* 设置主板控制器为 4 线模式 */
void hci_set_bus_width_4bit(void);
/* 切换主板进入 Default Speed 模式 (24MHz) */
int hci_set_default_speed(void);
/* 切换主板进入 High Speed 模式 (48MHz) */
int hci_set_high_speed(void);


/* =========================================================================
 * 2. 命令与响应引擎 (Command & Response Engine)
 * 负责控制 CMD 线路，向 SD 卡发射协议指令，并提取卡片回复的状态字。
 * ========================================================================= */
int hci_send_cmd(unsigned short cmd, unsigned int arg);
// 提取 32 位响应数据（最常用的 R1, R3, R6 等都在 Response Register 0）
unsigned int hci_get_response_0(void);


/* =========================================================================
 * 3. 数据传输与 FIFO 管理 (Data Transfer & FIFO Management)
 * 负责控制 DAT 线路，配置读写硬件状态机，并与控制器的内部缓存打交道。
 * ========================================================================= */
// 准备硬件传输环境 (块大小、数量、读写方向)
void hci_prepare_data_transfer(unsigned short blksize, unsigned short blkcnt, int is_read);
// 阻塞等待硬件 FIFO 准备就绪
int hci_wait_buffer_read_ready(void);
// 榨取 FIFO 数据到内存
void hci_read_fifo_data(unsigned int *buffer, int words);
// 阻塞等待物理线路传输彻底落幕
int hci_wait_transfer_complete(void);
