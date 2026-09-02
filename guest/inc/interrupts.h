#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#define PORT_SHARED 0x510
#define PORT_ACK 0x520
#define SHARED_BUF_SIZE 1024

void init_idt(void);

#endif /* INTERRUPTS_H */
