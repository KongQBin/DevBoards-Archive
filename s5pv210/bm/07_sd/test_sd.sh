#!/bin/bash
echo "$1"

# 使用 printf 生成带换行符 (\n) 和空字符终止符 (\0) 的文件
printf "Hello, S5PV210! This is the pure OS Payload loaded from SD Card Sector 100. Memory is fully under control!\n\0" > payload_test.bin

# 注意：这里有个小笔误修复。你在原文中写的是 of={$1}，大括号会导致设备名变成 /dev/{sdb} 而报错。
# 应该去掉大括号，写成 of=$1 或者 of="${1}"
sudo dd if=payload_test.bin of="${1}" bs=512 seek=100 conv=fsync

rm -f ./payload_test.bin
