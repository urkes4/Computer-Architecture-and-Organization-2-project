OUTPUT_FORMAT(binary)
SECTIONS
{
        . = 0x8000;
        .start : AT(0) { *(.start) }
        .text : { *(.text*) }
        .rodata : { *(.rodata) }
        .data : { *(.data) }
        .bss : { *(.bss*) }
}
