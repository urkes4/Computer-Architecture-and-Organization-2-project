#include "descriptors.h"
#include "interrupts.h"
#include "io.h"

const char *g_input_file  = "input.txt";
const char *g_output_file = "output.txt";

static struct gdt_entry gdt[3];

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void)
{
    struct dt_ptr p;

    gdt[0] = (struct gdt_entry){ 0 };
    gdt[1] = (struct gdt_entry){
        .limit_low   = 0xFFFF,
        .access      = 0x9A,
        .flags_limit = 0xAF,
    };
    gdt[2] = (struct gdt_entry){
        .limit_low   = 0xFFFF,
        .access      = 0x92,
        .flags_limit = 0xCF,
    };

    p.limit = sizeof(gdt) - 1;
    p.base  = (uint64_t)(uintptr_t)gdt;
    asm volatile("lgdt %0" : : "m"(p) : "memory");

    asm volatile(
        "pushq $0x08\n\t"
        "lea 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        ::: "rax", "memory"
    );

    asm volatile(
        "movl $0x10, %%eax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%ss\n\t"
        ::: "eax", "memory"
    );

    init_idt();
    asm volatile("sti");

    for (;;)
        asm volatile("hlt");
}
