ENTRY(_start)

SECTIONS
{
    . = 0xD0020010;

    .text : ALIGN(4)
    {
        /* 强制将入口代码放在最前面 */
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
