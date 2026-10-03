ENTRY(_start)

SECTIONS
{
    . = 0xD0020010;

    .text : ALIGN(4)
    {
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
