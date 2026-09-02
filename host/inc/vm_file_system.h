#ifndef VM_FILE_SYSTEM_H
#define VM_FILE_SYSTEM_H

#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX_ENTRIES      256
#define MAX_SHARED_FILES 64

enum OPERATION {
    NONE = 0, OPEN, CLOSE, READ, WRITE, LSEEK
};

struct file_table_entry {
    int v;
    int host_fd;
};

struct file_table {
    struct file_table_entry entries[MAX_ENTRIES + 1];
};

struct shared_file_table {
    char names[MAX_SHARED_FILES][256];
    int  count;
};

struct io_request {
    enum OPERATION op;

    int   path_len;
    char *path;
    int   path_index;
    int   flags;

    int   fd;
    int   count;
    char *buf;
    int   buf_index;

    int   offset;
    int   off_flag;

    int   result;
    int   result_ready;
};

int allocate_entry(struct file_table *ft);
int get_host_fd(struct file_table *ft, int guest_fd);
int map_guest_open_flags(int guest_flags);
int map_guest_seek_flag(int off_flag);
int is_valid_filename(const char *name);
int is_shared_file(struct shared_file_table *sft, const char *name);

void handle_fs_io(struct io_request *req, int direction,
                  uint32_t data_out, uint32_t *data_in,
                  struct file_table *ft,
                  struct shared_file_table *sft, int vm_index);

#endif /* VM_FILE_SYSTEM_H */
