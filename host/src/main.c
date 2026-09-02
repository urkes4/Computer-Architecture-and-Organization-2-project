#include "vm.h"
#include "vm_file_system.h"
#include "vm_irq.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_IMAGES 10

static int string_len(char *s) {
	int i = 0;
	while (s[i]) i++;
	return i;
}

struct vm_args {
	char   *image;
	size_t  memory_size;
	size_t  page_size;
	int     vm_index;
	struct shared_buf        *shared;
	struct shared_file_table *sft;
};

static void handle_io_port(struct kvm_run *run,
                            struct io_request *req, struct file_table *ft,
                            struct irq_io *irq, int vm_index,
                            struct shared_buf *shared,
                            struct shared_file_table *sft) {
	uint16_t port     = run->io.port;
	void    *data_ptr = (char *)run + run->io.data_offset;
	uint32_t data_out = 0;
	uint32_t data_in  = 0;
	int      dir      = run->io.direction;

	if (dir == KVM_EXIT_IO_OUT) {
		if (run->io.size == 1)
			data_out = *(uint8_t *)data_ptr;
		else
			data_out = *(uint32_t *)data_ptr;
	}

	if (port == 0xE9) {
		if (dir == KVM_EXIT_IO_OUT)
			printf("%c", (char)(data_out & 0xFF));
		return;
	}

	if (port == 0x0278) {
		handle_fs_io(req, dir, data_out, &data_in, ft, sft, vm_index);
	} else if (port == 0x510) {
		handle_irq_port(irq, dir, run->io.size, data_out, &data_in,
		                vm_index, shared);
	} else if (port == 0x520) {
		handle_ack_port(irq, dir, data_out, &data_in, shared);
	}

	if (dir == KVM_EXIT_IO_IN) {
		if (run->io.size == 1)
			*(uint8_t *)data_ptr = (uint8_t)data_in;
		else
			*(uint32_t *)data_ptr = data_in;
	}
}

void *vm_wrapper(void *args) {
	struct vm        v;
	struct vm_args  *va         = (struct vm_args *)args;
	struct io_request request;
	struct file_table file_table;
	struct irq_io     irq;
	struct kvm_sregs  sregs;
	struct kvm_regs   regs;
	int stop       = 0;
	int irq_inject = 1;

	memset(&request,    0, sizeof(request));
	memset(&file_table, 0, sizeof(file_table));
	memset(&irq,        0, sizeof(irq));
	irq.mode = -1;
	irq.idx  = -1;

	if (vm_init(&v, 1024u * 1024u * va->memory_size)) {
		printf("Failed to init VM %d\n", va->vm_index);
		return NULL;
	}

	if (ioctl(v.vcpu_fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		vm_destroy(&v);
		return NULL;
	}

	setup_long_mode(&v, &sregs, va->page_size);

	if (ioctl(v.vcpu_fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		vm_destroy(&v);
		return NULL;
	}

	if (load_guest_image(&v, va->image, GUEST_START_ADDR) < 0) {
		printf("Failed to load guest image for VM %d\n", va->vm_index);
		vm_destroy(&v);
		return NULL;
	}

	memset(&regs, 0, sizeof(regs));
	regs.rflags = 0x2;
	regs.rip    = GUEST_START_ADDR;
	regs.rsp    = 1024u * 1024u * va->memory_size;

	if (ioctl(v.vcpu_fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		vm_destroy(&v);
		return NULL;
	}

	v.run->request_interrupt_window = irq_inject;

	while (stop == 0) {
		int ret = ioctl(v.vcpu_fd, KVM_RUN, 0);
		if (ret == -1) {
			perror("KVM_RUN");
			vm_destroy(&v);
			return NULL;
		}

		switch (v.run->exit_reason) {
		case KVM_EXIT_IO:
			handle_io_port(v.run, &request, &file_table,
			               &irq, va->vm_index, va->shared, va->sft);
			continue;

		case KVM_EXIT_IRQ_WINDOW_OPEN:
			if (irq_inject) {
				if (inject_irq(&v, IRQ_NUM) < 0) {
					vm_destroy(&v);
					return NULL;
				}
				irq_inject = 0;
				v.run->request_interrupt_window = 0;
			}
			continue;

		case KVM_EXIT_HLT: {
			struct kvm_regs r;
			if (ioctl(v.vcpu_fd, KVM_GET_REGS, &r) < 0) {
				perror("KVM_GET_REGS");
				stop = 1;
				break;
			}
			if (!(r.rflags & (1u << 9))) {
				printf("VM %d done\n", va->vm_index);
				stop = 1;
			} else if (irq.mode == -1 || irq.cycles < ITERATIONS) {
				irq_inject = 1;
				v.run->request_interrupt_window = 1;
			} else {
				printf("VM %d done (%d cycles)\n", va->vm_index, irq.cycles);
				stop = 1;
			}
			break;
		}

		case KVM_EXIT_SHUTDOWN:
			printf("VM %d: shutdown\n", va->vm_index);
			stop = 1;
			break;

		default:
			printf("VM %d: unexpected exit reason %d\n",
			       va->vm_index, v.run->exit_reason);
			stop = 1;
			break;
		}
	}

	vm_destroy(&v);
	free(args);
	return NULL;
}

int main(int argc, char *argv[]) {
	if (argc < 7) {
		printf("Usage: hypervisor --memory <2|4|8> --page <4|2> --guest <img...> [--files <f...>]\n");
		return 1;
	}

	int    i;
	size_t memory_size  = 0;
	size_t page_size    = 0;
	char  *images[MAX_IMAGES];
	int    images_count = 0;
	struct shared_file_table shared_files;
	memset(&shared_files, 0, sizeof(shared_files));

	for (i = 1; i < argc; ) {
		if (strcmp(argv[i], "--memory") == 0 || strcmp(argv[i], "-m") == 0) {
			i++;
			if (i == argc) { printf("Missing memory size\n"); return 1; }
			memory_size = (size_t)atoi(argv[i++]);
		} else if (strcmp(argv[i], "--page") == 0 || strcmp(argv[i], "-p") == 0) {
			i++;
			if (i == argc) { printf("Missing page size\n"); return 1; }
			page_size = (size_t)atoi(argv[i++]);
		} else if (strcmp(argv[i], "--guest") == 0 || strcmp(argv[i], "-g") == 0) {
			i++;
			while (i < argc && argv[i][0] != '-') {
				if (images_count < MAX_IMAGES) {
					images[images_count] = malloc((size_t)string_len(argv[i]) + 1);
					strcpy(images[images_count++], argv[i]);
				}
				i++;
			}
		} else if (strcmp(argv[i], "--files") == 0 || strcmp(argv[i], "-f") == 0) {
			i++;
			while (i < argc && argv[i][0] != '-') {
				if (shared_files.count < MAX_SHARED_FILES) {
					strncpy(shared_files.names[shared_files.count],
					        argv[i], 255);
					shared_files.names[shared_files.count][255] = '\0';
					shared_files.count++;
				}
				i++;
			}
		} else {
			i++;
		}
	}

	if (memory_size != 2 && memory_size != 4 && memory_size != 8) {
		printf("Invalid memory size (allowed: 2, 4, 8 MB)\n");
		return 1;
	}
	if (page_size != 4 && page_size != 2) {
		printf("Invalid page size (allowed: 4 KB or 2 MB)\n");
		return 1;
	}
	if (images_count == 0) {
		printf("No guest images provided\n");
		return 1;
	}

	printf("Memory: %lu MB  Page: %lu KB  VMs: %d  Shared files: %d\n",
	       memory_size, page_size, images_count, shared_files.count);

	struct shared_buf shared;
	int num_readers = images_count - 1;
	shared_buf_init(&shared, num_readers > 0 ? num_readers : 1);

	pthread_t threads[MAX_IMAGES];
	for (i = 0; i < images_count; i++) {
		struct vm_args *arg = malloc(sizeof(*arg));
		arg->image       = images[i];
		arg->page_size   = page_size;
		arg->memory_size = memory_size;
		arg->vm_index    = i;
		arg->shared      = &shared;
		arg->sft         = &shared_files;
		pthread_create(&threads[i], NULL, vm_wrapper, arg);
	}

	for (i = 0; i < images_count; i++)
		pthread_join(threads[i], NULL);

	shared_buf_destroy(&shared);
	return 0;
}
