# 1. Compiler Setup
CC := gcc
# -MMD -MP: KEY!!! Announce GCC to create Dependencies (.d)
# -Wall -Wextra: Open all warnings
# -g: Debugger
# -pthread: Thread support
# -Iinclude: Header path
CFLAGS := -Wall -Wextra -g -pthread -Iinclude -MMD -MP

# 2. Set File and Path
TARGET := c_thread_pool_demo
SRC_DIR := src
OBJ_DIR := obj

# Automatic catch all .c files under src (We don't need to write manually!!)
SRCS := $(wildcard $(SRC_DIR)/*.c)
# Change .c into .o，and add obj/ prefix (Ex: src/main.c -> obj/main.o)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
# Generate corresponding Dependencies List (.d)
DEPS := $(OBJS:.o=.d)

# 3. Primary Rule
.PHONY: all clean directories

all: directories $(TARGET)

# Linking
$(TARGET): $(OBJS)
	@echo "Linking $@"
	$(CC) $(CFLAGS) -o $@ $^

# Compiling
# $@ refers to (.o), $< refers to first dependencies (.c)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Create obj directory (If hasn't exist)
directories:
	@mkdir -p $(OBJ_DIR)

# 4. Pull in the dependencies (KEY!!!)
# If .d exists，make will read it，dynamically add dependencies
-include $(DEPS)

# 5. Clear Rule
clean:
	@echo "Cleaning up..."
	rm -rf $(OBJ_DIR) $(TARGET)
