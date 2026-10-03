ENTRY(_start)

SECTIONS
{
    /*
     * SD 卡启动时，iROM 会将 16KB (含 16 字节头部) 复制到 0xD0020000
     * 实际代码紧跟在 16 字节头部之后，因此基地址为 0xD0020010
     */
    . = 0xD0020010;

    .text : ALIGN(4)
    {
        KEEP(*(.text.entry))

        *(.text*)
        *(.rodata*)
    }

    .data : ALIGN(4)
    {
        *(.data*)
    }

    .bss : ALIGN(4)
    {
        *(.bss*)
        *(COMMON)
    }
}
