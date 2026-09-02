#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include "io.h"

#define O_RD 1
#define O_WR 2
#define O_RDWR 4
#define O_CREATE 8

#define SEEK_SET 1
#define SEEK_END 2

#define IO_PORT 0x0278


// Enum for operation codes
enum OPERATION{
    NONE=0, OPEN, CLOSE, READ, WRITE, LSEEK
};

int open(const char *path, int flags);
int close(int fd);
int read(int fd, char *buf, int count);
int write(int fd, const char *buf, int count);
int lseek(int fd, const int offset, int off_flag);

#endif