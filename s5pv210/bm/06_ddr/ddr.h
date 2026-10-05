#pragma once
#include "ddr_test.h"
/*
 * S5PV210 DDR2 初始化
 *
 * 当前版本：
 *   - 面向 Study210 / X210
 *   - 裸机模式
 *   - 先初始化 DMC0
 *   - DMC0 映射到 0x20000000
 *   - 目标容量：256MB
 *
 * 注意：
 *   Study210 板载总 DDR2 为 512MB，
 *   本文件当前只把 DMC0 这一半用于裸机实验。
 *
 * 等 DMC0 稳定以后，再扩展 DMC1。
 */


/*
 * 初始化 DDR2
 */
void ddr_init(void);
