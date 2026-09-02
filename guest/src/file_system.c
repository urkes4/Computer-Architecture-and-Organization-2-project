#include "file_system.h"

static int str_len(const char *str) {
    int i = 0;
    while (str[i] != '\0') i++;
    return i;
}

// op -> path_len -> path -> flags
int open(const char *path, int flags) {
    int len = str_len(path);
    int i;
    outb(IO_PORT, OPEN);
    outl(IO_PORT, (uint32_t)len);
    for (i = 0; i < len; i++)
        outb(IO_PORT, (uint8_t)path[i]);
    outl(IO_PORT, (uint32_t)flags);
    return (int)inl(IO_PORT);
}

// op -> fd
int close(int fd) {
    outb(IO_PORT, CLOSE);
    outl(IO_PORT, (uint32_t)fd);
    return (int)inl(IO_PORT);
}

// op -> fd -> count
int read(int fd, char *buf, int count) {
    int i;
    outb(IO_PORT, READ);
    outl(IO_PORT, (uint32_t)fd);
    outl(IO_PORT, (uint32_t)count);
    int bytes_read = (int)inl(IO_PORT);
    if (bytes_read <= 0)
        return bytes_read;
    for (i = 0; i < bytes_read; i++)
        buf[i] = (char)inb(IO_PORT);
    return bytes_read;
}

// op -> fd -> count -> buf
int write(int fd, const char *buf, int count) {
    int i;
    outb(IO_PORT, WRITE);
    outl(IO_PORT, (uint32_t)fd);
    outl(IO_PORT, (uint32_t)count);
    for (i = 0; i < count; i++)
        outb(IO_PORT, (uint8_t)buf[i]);
    return (int)inl(IO_PORT);
}

// op -> fd -> offset -> flags
int lseek(int fd, const int offset, int off_flag) {
    outb(IO_PORT, LSEEK);
    outl(IO_PORT, (uint32_t)fd);
    outl(IO_PORT, (uint32_t)offset);
    outl(IO_PORT, (uint32_t)off_flag);
    return (int)inl(IO_PORT);
}
