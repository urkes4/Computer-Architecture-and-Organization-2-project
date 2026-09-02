CC = gcc

BUILD_DIR ?= build

COMMON_SRCS = src/file_system.c src/interrupts.c
COMMON_OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(COMMON_SRCS))

ALL_SRCS = $(wildcard src/*.c)
DEPS     = $(patsubst src/%.c,$(BUILD_DIR)/%.d,$(ALL_SRCS))

IMGS = $(BUILD_DIR)/guest.img  \
       $(BUILD_DIR)/guest2.img \
       $(BUILD_DIR)/guest3.img \
       $(BUILD_DIR)/guest4.img

.PHONY: all clean

all: $(BUILD_DIR) $(IMGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) -m64 -ffreestanding -fno-pic -Iinc -MMD -MP -c -o $@ $<

$(BUILD_DIR)/guest.img:  $(BUILD_DIR)/main.o  $(COMMON_OBJS) guest.ld
	ld -T guest.ld $(BUILD_DIR)/main.o  $(COMMON_OBJS) -o $@

$(BUILD_DIR)/guest2.img: $(BUILD_DIR)/main2.o $(COMMON_OBJS) guest.ld
	ld -T guest.ld $(BUILD_DIR)/main2.o $(COMMON_OBJS) -o $@

$(BUILD_DIR)/guest3.img: $(BUILD_DIR)/main3.o $(COMMON_OBJS) guest.ld
	ld -T guest.ld $(BUILD_DIR)/main3.o $(COMMON_OBJS) -o $@

$(BUILD_DIR)/guest4.img: $(BUILD_DIR)/main4.o $(COMMON_OBJS) guest.ld
	ld -T guest.ld $(BUILD_DIR)/main4.o $(COMMON_OBJS) -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
