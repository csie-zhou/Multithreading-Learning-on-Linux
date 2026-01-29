CC = gcc
# -Wall: Open warning, -g: including error infos (for Valgrind/GDB), -pthread: support POSIX Threads
CFLAGS = -Wall -Wextra -g -pthread -Iinclude

# Target files
TARGET = c_thread_pool_demo
SRC = src/main.c src/thread_pool.c
OBJS = src/main.o src/thread_pool.o

# Default target
all: $(TARGET)

# Linking
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compiling
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
