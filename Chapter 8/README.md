# Chapter 8. Build System
In Chapter 1, we wrote a simple Makefile. There will be some fatal errors if we not properly handle in other files. These Bugs can also be invisible by this Makefile.    
## Key Tasks
1. Find the error in current Makefile
2. Generate a **"Automatic Dependency Generation"** Makefile  

## Find the error in current Makefile
### 1. Test the Bug
1. Execute `make` in terminal, check if it is the newest. *(Output: make: Nothing to be done for 'all'.)*
2. Modify header file: Open `include/thread_pool.h`, add a random space line or annotations (Eg. // Test) and save.
3. Execute `make` again.
  - Expected: This should recompile to all files that included `thread_pool.h` (`main.c`, `thread_pool.c`).
  - Actual: `make: Nothing to be done for 'all'.`  

**Result:** In bigger program, if someone changes the Struct (`.h`), but Makefile does not recompile `.c`. The program will suffer in Memory Malposition and casue Random Crash. Which is also difficult to debug.  

## Generate a "Automatic Dependency Generation" Makefile
To conquer the above problem, we use `-MMD` in **GCC**.  

We don't manually write `main.o: main.c include/thread_pool.h` in Makefile. We call GCC compiler to create a **Dependencies File (.d)** while compiling. (Like Python creating `requirements.txt`). 
### 1. Modify Makefile
Copy paste this in the current `Makefile`, beware of indent:
```Makefile
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
```
Command Line in Makefile (Ex. `&(CC)` and `rm` ...), indent should be **Tab**, not space.  

## Test Again
### 1. Compile
```
make clean
make
```
There will be a new folder `obj/` with `.o` and `.d` files. In `.d` files, GCC automatically create lines to include the header (`.h`) for all `.c`.
### 2. Run the Test Again
1. Execute `make`, shows "Nothing to be done"
2. Modify `include/thread_pool.h`
3. Execute `make` again.

### 3. Expected
```
Compiling src/main.c
Compiling src/thread_pool.c
Linking c_thread_pool_demo
```


