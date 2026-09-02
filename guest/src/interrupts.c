#include "descriptors.h"
#include "interrupts.h"
#include "file_system.h"
#include "io.h"

static struct idt_entry idt[IDT_ENTRIES];

extern const char *g_input_file;
extern const char *g_output_file;

static volatile int g_mode   = -1;
static volatile int g_cycles = 0;

static void print(const char *s) {
    while (*s) outb(0xE9, (uint8_t)*s++);
}

static void print_int(int n) {
    char buf[12];
    int i = 11;
    buf[i] = '\0';
    if (n == 0) { outb(0xE9, '0'); return; }
    while (n > 0) { buf[--i] = (char)('0' + n % 10); n /= 10; }
    print(&buf[i]);
}

static void __attribute__((interrupt, target("general-regs-only")))
irq32_handler(struct interrupt_frame *frame)
{
    (void)frame;

	// No mode - we give him his work mode
    if (g_mode == -1) {
        g_mode = (int)inl(PORT_SHARED);
        return;
    }

	// writer mode
    if (g_mode == 1) {
        char buf[SHARED_BUF_SIZE];
        int n = 0;

        int fd = g_input_file ? open(g_input_file, O_RD) : -1;
        if (fd >= 0) {
            n = read(fd, buf, SHARED_BUF_SIZE);
            close(fd);
        }

        print("[Writer] ciklus "); print_int(g_cycles + 1);
        print(", salje "); print_int(n); print(" bajta\n");

        outl(PORT_SHARED, (uint32_t)n);
        int i;
        for (i = 0; i < n; i++)
            outb(PORT_SHARED, (uint8_t)buf[i]);

        inl(PORT_ACK);
        print("[Writer] svi readeri procitali\n");
		
    } 
	// reader mode
	else {
        int count = (int)inl(PORT_SHARED);
        if (count <= 0) {
            outl(PORT_ACK, 0);
            return;
        }

        char buf[SHARED_BUF_SIZE];
        int i;
        for (i = 0; i < count; i++)
            buf[i] = (char)inb(PORT_SHARED);

        print("[Reader] ciklus "); print_int(g_cycles + 1);
        print(", primio "); print_int(count); print(" bajta\n");

        if (g_output_file) {
            int fd = open(g_output_file, O_WR | O_CREATE);
            if (fd >= 0) {
                lseek(fd, 0, SEEK_END);
                write(fd, buf, count);
                close(fd);
            }
        }

        outl(PORT_ACK, (uint32_t)count);
    }

    g_cycles++;
}

static void set_idt_gate(unsigned n, void (*handler)(struct interrupt_frame *)) {
    uint64_t addr = (uint64_t)(uintptr_t)handler;
    idt[n].offset_low  = addr & 0xFFFF;
    idt[n].selector    = 0x08;
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E;
    idt[n].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].reserved    = 0;
}

void init_idt(void) {
    struct dt_ptr p;
    set_idt_gate(32, irq32_handler);
    p.limit = sizeof(idt) - 1;
    p.base  = (uint64_t)(uintptr_t)idt;
    asm volatile("lidt %0" : : "m"(p) : "memory");
}
