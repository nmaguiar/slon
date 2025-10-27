CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2
INCLUDES = -Iinclude
BUILD_DIR = build

LIB = $(BUILD_DIR)/libslon.a
OBJS = $(BUILD_DIR)/slon.o

all: $(LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/slon.o: src/slon.c include/slon.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIB): $(OBJS)
	ar rcs $@ $^

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
