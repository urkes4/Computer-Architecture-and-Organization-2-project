#include "vm_file_system.h"

int allocate_entry(struct file_table *ft) {
    int i;
    for (i = 1; i <= MAX_ENTRIES; i++) {
        if (!ft->entries[i].v)
            return i;
    }
    return -1;
}

int get_host_fd(struct file_table *ft, int guest_fd) {
    if (guest_fd < 1 || guest_fd > MAX_ENTRIES)
        return -1;
    if (!ft->entries[guest_fd].v)
        return -1;
    return ft->entries[guest_fd].host_fd;
}

int map_guest_open_flags(int guest_flags) {
    int host_flags = 0;
    int access = guest_flags & 0x7;

    if (access & 4)
        host_flags |= O_RDWR;
    else if (access & 2)
        host_flags |= O_WRONLY;

    if (guest_flags & 8)
        host_flags |= O_CREAT;

    return host_flags;
}

int map_guest_seek_flag(int off_flag) {
    if (off_flag == 2) return SEEK_END;
    return SEEK_SET;
}

int is_valid_filename(const char *name) {
    int i;
    if (!name || !name[0]) return 0;
    if (!isalpha((unsigned char)name[0])) return 0;
    for (i = 1; name[i]; i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '.')
            return 0;
    }
    return 1;
}

int is_shared_file(struct shared_file_table *sft, const char *name) {
    int i;
    if (!sft || !name) return 0;
    for (i = 0; i < sft->count; i++) {
        if (strcmp(sft->names[i], name) == 0)
            return 1;
    }
    return 0;
}

static void copy_file_for_vm(const char *src, const char *dst) {
    char buf[4096];
    ssize_t n;
    int fd_r = open(src, O_RDONLY);
    int fd_w = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_r >= 0 && fd_w >= 0) {
        while ((n = read(fd_r, buf, sizeof(buf))) > 0)
            write(fd_w, buf, (size_t)n);
    }
    if (fd_r >= 0) close(fd_r);
    if (fd_w >= 0) close(fd_w);
}

static void reset_request(struct io_request *req) {
    if (req->path) free(req->path);
    if (req->buf)  free(req->buf);
    memset(req, 0, sizeof(*req));
}

void handle_fs_io(struct io_request *req, int direction,
                  uint32_t data_out, uint32_t *data_in,
                  struct file_table *ft,
                  struct shared_file_table *sft, int vm_index) {

    if (direction == 1) {

        switch (req->op) {

        case NONE:
            req->op = (enum OPERATION)(data_out & 0xFF);
            break;

        case OPEN:
            if (!req->path_len) {
                req->path_len   = (int)data_out;
                req->path_index = 0;
                req->path       = malloc((size_t)(req->path_len + 1));
            } else if (req->path_index < req->path_len) {
                req->path[req->path_index++] = (char)(data_out & 0xFF);
            } else {
                req->path[req->path_len] = '\0';
                req->flags = (int)data_out;

                if (!is_valid_filename(req->path)) {
                    req->result = -1;
                } else {
                    int hflags = map_guest_open_flags(req->flags);
                    const char *open_path = req->path;
                    char copy_path[300];

                    if (is_shared_file(sft, req->path) && (req->flags & 0x6)) {
                        snprintf(copy_path, sizeof(copy_path),
                                 "vm%d_%s", vm_index, req->path);
                        copy_file_for_vm(req->path, copy_path);
                        open_path = copy_path;
                        hflags   |= O_TRUNC;
                    }

                    int host_fd = open(open_path, hflags, 0644);
                    if (host_fd < 0) {
                        req->result = -1;
                    } else {
                        int gfd = allocate_entry(ft);
                        if (gfd < 0) {
                            close(host_fd);
                            req->result = -1;
                        } else {
                            ft->entries[gfd].v       = 1;
                            ft->entries[gfd].host_fd = host_fd;
                            req->result = gfd;
                        }
                    }
                }
                free(req->path);
                req->path         = NULL;
                req->result_ready = 1;
            }
            break;

        case CLOSE:
            if (!req->fd) {
                req->fd = (int)data_out;
                int host_fd = get_host_fd(ft, req->fd);
                if (host_fd < 0) {
                    req->result = -1;
                } else {
                    close(host_fd);
                    ft->entries[req->fd].v       = 0;
                    ft->entries[req->fd].host_fd = -1;
                    req->result = 0;
                }
                req->result_ready = 1;
            }
            break;

        case READ:
            if (!req->fd) {
                req->fd        = (int)data_out;
                req->buf_index = -1;
            } else if (req->buf_index == -1) {
                req->count = (int)data_out;
                int host_fd = get_host_fd(ft, req->fd);
                if (host_fd < 0 || req->count <= 0) {
                    req->result = -1;
                    req->buf    = NULL;
                } else {
                    req->buf = malloc((size_t)req->count);
                    int n    = (int)read(host_fd, req->buf, (size_t)req->count);
                    req->result = n;
                    if (n <= 0) { free(req->buf); req->buf = NULL; }
                }
                req->buf_index    = 0;
                req->result_ready = 1;
            }
            break;

        case WRITE:
            if (!req->fd) {
                req->fd        = (int)data_out;
                req->buf_index = -1;
            } else if (req->buf_index == -1) {
                req->count     = (int)data_out;
                req->buf_index = 0;
                if (req->count == 0) {
                    req->result       = 0;
                    req->result_ready = 1;
                } else {
                    req->buf = malloc((size_t)req->count);
                }
            } else {
                req->buf[req->buf_index++] = (char)(data_out & 0xFF);
                if (req->buf_index == req->count) {
                    int host_fd = get_host_fd(ft, req->fd);
                    if (host_fd < 0) {
                        req->result = -1;
                    } else {
                        req->result = (int)write(host_fd, req->buf,
                                                 (size_t)req->count);
                    }
                    free(req->buf);
                    req->buf          = NULL;
                    req->result_ready = 1;
                }
            }
            break;

        case LSEEK:
            if (!req->fd) {
                req->fd         = (int)data_out;
                req->path_index = 0;
            } else if (!req->path_index) {
                req->offset     = (int)data_out;
                req->path_index = 1;
            } else {
                req->off_flag = (int)data_out;
                int host_fd   = get_host_fd(ft, req->fd);
                if (host_fd < 0) {
                    req->result = -1;
                } else {
                    req->result = (int)lseek(host_fd, (off_t)req->offset,
                                             map_guest_seek_flag(req->off_flag));
                }
                req->result_ready = 1;
            }
            break;

        default:
            reset_request(req);
            break;
        }

    } else {

        if (req->result_ready) {
            *data_in = (uint32_t)req->result;
            req->result_ready = 0;
            if (req->op != READ || req->buf == NULL)
                reset_request(req);
        } else if (req->op == READ && req->buf != NULL) {
            *data_in = (uint32_t)(unsigned char)req->buf[req->buf_index++];
            if (req->buf_index == req->result)
                reset_request(req);
        } else {
            *data_in = (uint32_t)-1;
        }
    }
}
