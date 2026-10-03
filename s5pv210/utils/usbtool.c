#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#define VENDOR_ID       0x04e8
#define PRODUCT_ID      0x1234

#define EP_OUT          0x02

// 朱有鹏 X210 裸机 USB-DNW 下载地址
#define LOAD_ADDR       0xD0020010

#define USB_TIMEOUT_MS  10000


static uint16_t calc_checksum(const unsigned char *data, size_t len)
{
    uint32_t sum = 0;

    for (size_t i = 0; i < len; ++i) {
        sum += data[i];
    }

    return (uint16_t)(sum & 0xFFFF);
}


static void put_le32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)(value & 0xFF);
    p[1] = (unsigned char)((value >> 8) & 0xFF);
    p[2] = (unsigned char)((value >> 16) & 0xFF);
    p[3] = (unsigned char)((value >> 24) & 0xFF);
}


static void put_le16(unsigned char *p, uint16_t value)
{
    p[0] = (unsigned char)(value & 0xFF);
    p[1] = (unsigned char)((value >> 8) & 0xFF);
}


int main(int argc, char **argv)
{
    libusb_context *ctx = NULL;
    libusb_device_handle *dev_handle = NULL;

    FILE *fp = NULL;

    unsigned char *buf = NULL;

    long file_len;
    size_t payload_len;
    size_t total_len;

    size_t nread;

    int transferred = 0;
    int r = 0;


    // 参数检查
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <binary_file>\n", argv[0]);
        return 1;
    }

    // 打开文件
    fp = fopen(argv[1], "rb");

    if (!fp) {
        perror("fopen");
        return 1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(fp);
        return 1;
    }

    file_len = ftell(fp);

    if (file_len < 0) {
        perror("ftell");
        fclose(fp);
        return 1;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        return 1;
    }

    payload_len = (size_t)file_len;
    if (payload_len == 0) {
        fprintf(stderr, "Binary file is empty\n");
        fclose(fp);
        return 1;
    }


    /*
     * DNW packet:
     *
     *   4 bytes  : download address
     *   4 bytes  : payload size
     *   N bytes  : payload
     *   2 bytes  : checksum
     *
     * 总长度 = N + 10
     */

    total_len = payload_len + 10;
    buf = malloc(total_len);
    if (!buf) {
        fprintf(stderr, "malloc failed\n");
        fclose(fp);
        return 1;
    }

    memset(buf, 0, total_len);
    // DNW 头
    put_le32(buf + 0, LOAD_ADDR);

    /*
     * 注意：
     *
     * 这里是原始 payload 长度，
     * 不是 payload + 10。
     */
    put_le32(buf + 4, (uint32_t)payload_len);

    // 读取 payload
    nread = fread(buf + 8, 1, payload_len, fp);
    fclose(fp);
    fp = NULL;

    if (nread != payload_len) {
        fprintf(stderr, "fread failed: expected %zu bytes, got %zu bytes\n", payload_len, nread);
        free(buf);
        return 1;
    }

    // 校验和
    uint16_t checksum = calc_checksum(buf + 8, payload_len);
    put_le16(buf + 8 + payload_len, checksum);

    // 输出信息
    printf("File           : %s\n",         argv[1]);
    printf("Payload size   : %zu bytes\n",  payload_len);
    printf("Load address   : 0x%08X\n",     LOAD_ADDR);
    printf("Checksum       : 0x%04X\n",     checksum);
    printf("USB total size : %zu bytes\n",  total_len);


    // libusb 初始化
    r = libusb_init(&ctx);

    if (r < 0) {
        fprintf(stderr, "libusb_init failed: %s\n", libusb_error_name(r));
        free(buf);
        return 1;
    }

    // 等待 X210
    printf("\nWaiting for X210 USB Boot device " "(%04x:%04x)...\n", VENDOR_ID, PRODUCT_ID);
    printf("Please put X210 into USB boot mode " "and power it on.\n");


    while (1) {
        dev_handle = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
        if (dev_handle) {
            break;
        }
        usleep(50 * 1000);
    }
    printf("Device found!\n");

    /*
    * X210 IROM USB Boot 在设备枚举后，
    * 可能还需要一点时间完成接收端初始化。
    */
    usleep(300000);

    // 卸载 kernel driver
    int active = libusb_kernel_driver_active(dev_handle, 0);
    if (active == 1) {
        r = libusb_detach_kernel_driver(dev_handle, 0);
        if (r < 0) {
            fprintf(stderr, "Warning: detach kernel driver failed: %s\n", libusb_error_name(r));
        }
    }

    // claim interface
    r = libusb_claim_interface(dev_handle, 0);
    if (r < 0) {
        fprintf(stderr, "libusb_claim_interface failed: %s\n", libusb_error_name(r));
        goto cleanup;
    }

    // 发送完整 DNW packet
    printf("Sending %zu bytes to 0x%08X via EP 0x%02X...\n", total_len, LOAD_ADDR, EP_OUT);
    r = libusb_bulk_transfer(dev_handle, EP_OUT, buf, (int)total_len, &transferred, USB_TIMEOUT_MS);

    if (r == 0) {
        printf("USB transfer completed: %d bytes.\n", transferred);
        printf("Download packet accepted by USB device.\n");
    } else {
        fprintf(stderr, "USB transfer failed: %s (%d)\n", libusb_error_name(r), r);
        fprintf(stderr, "Transferred: %d / %zu bytes\n", transferred, total_len);
    }

    libusb_release_interface(dev_handle, 0);


cleanup:

    if(dev_handle) libusb_close(dev_handle);
    if(ctx) libusb_exit(ctx);
    free(buf);
    return (r == 0) ? 0 : 1;
}
