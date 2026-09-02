#ifndef IO_H
#define IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value)
{
	asm("outb %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}

static inline void outl(uint16_t port, uint32_t value)
{
	asm("outl %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}

static inline uint32_t inl(uint16_t port)
{
	uint32_t ret;
	asm("inl %1,%0" : "=a" (ret) : "Nd" (port) : "memory");
	return ret;
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t ret;
	asm("inb %1,%0" : "=a" (ret) : "Nd" (port) : "memory");
	return ret;
}

#endif /* IO_H */
