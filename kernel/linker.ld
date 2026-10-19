ENTRY(_start)

PHDRS
{
    text   PT_LOAD FLAGS(5);   /* R + X  */
    rodata PT_LOAD FLAGS(4);   /* R      */
    data   PT_LOAD FLAGS(6);   /* RW     */
}

SECTIONS
{
    . = 1M;

    .text :
    {
        KEEP(*(.multiboot))
        *(.text)
    } :text

    .rodata :
    {
        *(.rodata*)
    } :rodata

    .data :
    {
        *(.data)
    } :data

    .bss :
    {
        *(COMMON)
        *(.bss)
    } :data
}
