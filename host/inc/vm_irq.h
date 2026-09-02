#ifndef VM_IRQ_H
#define VM_IRQ_H

#include <semaphore.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE  1024
#define ITERATIONS   3

struct shared_buf {
    char data[BUFFER_SIZE];
    int len;
    int num_readers;
    sem_t sem_write;
    sem_t sem_read;
};

struct irq_io {
    int  mode;               // -1=unset, 0=reader, 1=writer
    int  count;              // bytes in current transfer
    int  idx;                // position in transfer; -1 = waiting for count
    char buf[BUFFER_SIZE];   // writer accumulates bytes here before flushing
    int  cycles;             // completed write/read cycles
};

void shared_buf_init(struct shared_buf *sb, int num_readers);
void shared_buf_destroy(struct shared_buf *sb);

void handle_irq_port(struct irq_io *irq, int direction,
                     uint8_t size, uint32_t data_out, uint32_t *data_in,
                     int vm_index, struct shared_buf *sb);

void handle_ack_port(struct irq_io *irq, int direction,
                     uint32_t data_out, uint32_t *data_in,
                     struct shared_buf *sb);

#endif
