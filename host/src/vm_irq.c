#include "vm_irq.h"

void shared_buf_init(struct shared_buf *sb, int num_readers) {
    memset(sb, 0, sizeof(*sb));
    sb->num_readers = num_readers;
    sem_init(&sb->sem_write, 0, (unsigned int)num_readers);
    sem_init(&sb->sem_read,  0, 0);
}

void shared_buf_destroy(struct shared_buf *sb) {
    sem_destroy(&sb->sem_write);
    sem_destroy(&sb->sem_read);
}


void handle_irq_port(struct irq_io *irq, int direction,
                     uint8_t size, uint32_t data_out, uint32_t *data_in,
                     int vm_index, struct shared_buf *sb) {

    if (irq->mode == -1) {
        if (direction == 0) {
            // logic of giving VM a role
            irq->mode = (vm_index == 0) ? 1 : 0;
            *data_in  = (uint32_t)irq->mode;
        }
        return;
    }
    if (irq->mode == 1) {  // writer: prima bajte od guesta, kopira u shared buf
        if (direction == 1) {
            if (irq->idx == -1) {
                irq->count = (int)data_out;
                irq->idx   = 0;
            } else {
                irq->buf[irq->idx++] = (char)(data_out & 0xFF);
                if (irq->idx == irq->count) {
                    int i, len;
                    for (i = 0; i < sb->num_readers; i++)
                        sem_wait(&sb->sem_write);
                    len = (irq->count < BUFFER_SIZE) ? irq->count : BUFFER_SIZE;
                    memcpy(sb->data, irq->buf, (size_t)len);
                    sb->len = len;
                    for (i = 0; i < sb->num_readers; i++)
                        sem_post(&sb->sem_read);
                    irq->idx   = -1;
                    irq->count = 0;
                }
            }
        }
    } else {  // reader: vraca bajte iz shared buf ka guestu
        if (direction == 0) {
            if (irq->idx == -1) {
                sem_wait(&sb->sem_read);
                irq->count = sb->len;
                irq->idx   = 0;
                *data_in   = (uint32_t)irq->count;
            } else {
                *data_in = (uint32_t)(unsigned char)sb->data[irq->idx++];
                if (irq->idx == irq->count) {
                    irq->idx   = -1;
                    irq->count = 0;
                }
            }
        }
    }
}

void handle_ack_port(struct irq_io *irq, int direction,
                     uint32_t data_out, uint32_t *data_in,
                     struct shared_buf *sb) {

    if (irq->mode == 1 && direction == 0) {
        *data_in = (uint32_t)sb->len;
        irq->cycles++;
    } else if (irq->mode == 0 && direction == 1) {
        sem_post(&sb->sem_write);
        irq->cycles++;
    }
}
