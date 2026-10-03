#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BL1_SIZE (16 * 1024)
#define HEADER_SIZE 16
#define PAYLOAD_MAX_SIZE (BL1_SIZE - HEADER_SIZE)

int main(int argc, char *argv[]) {
    FILE *fp_in, *fp_out;
    unsigned char *buf;
    unsigned int file_len, checksum = 0;
    int i;

    if (argc != 3) {
        printf("Usage: %s <input.bin> <output.bin>\n", argv[0]);
        return -1;
    }

    fp_in = fopen(argv[1], "rb");
    if (!fp_in) {
        perror("Error opening input file");
        return -1;
    }

    fp_out = fopen(argv[2], "wb");
    if (!fp_out) {
        perror("Error opening output file");
        fclose(fp_in);
        return -1;
    }

    // 分配 16KB 的缓冲区并全部初始化为 0
    buf = (unsigned char *)calloc(1, BL1_SIZE);
    if (!buf) {
        printf("Memory allocation failed!\n");
        fclose(fp_in);
        fclose(fp_out);
        return -1;
    }

    // 读取输入文件内容
    file_len = fread(buf + HEADER_SIZE, 1, PAYLOAD_MAX_SIZE, fp_in);

    // 不管输入的 bin 文件有多小，强制将头部声明的有效载荷大小设为满载的 PAYLOAD_MAX_SIZE
    // 这样 iROM 就会老老实实把整整 16KB 全部读进 SRAM，彻底杜绝读盘残缺
    unsigned int target_size = PAYLOAD_MAX_SIZE;

    // 计算从头到尾（包含填充的 0）的完整校验和
    for (i = 0; i < target_size; i++) {
        checksum += buf[HEADER_SIZE + i];
    }

    // 填充 16 字节头部 (小端序)
    // [0-3]: 长度, [4-7]: 保留0, [8-11]: 校验和, [12-15]: 保留0
    *((unsigned int *)buf) = target_size;
    *((unsigned int *)(buf + 8)) = checksum;

    // 写入完整的 16KB 输出文件
    fwrite(buf, 1, BL1_SIZE, fp_out);

    printf("Success: Generated %s (Forced Payload: %u bytes, Checksum: 0x%08X)\n", argv[2], target_size, checksum);

    free(buf);
    fclose(fp_in);
    fclose(fp_out);
    return 0;
}
