CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g -Iinclude
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
    LDFLAGS = -lpthread
else
    LDFLAGS = -lpthread -lrt
endif

SRC     = src/scheduler.c src/rt_mutex.c
OBJ     = $(SRC:.c=.o)

DEMOS   = demo_periodic demo_priority_inversion demo_overload

.PHONY: all clean run_periodic run_pip run_overload

all: $(DEMOS)

demo_periodic: $(OBJ) demos/demo_periodic.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_priority_inversion: $(OBJ) demos/demo_priority_inversion.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_overload: $(OBJ) demos/demo_overload.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run_periodic: demo_periodic
	@echo "\n=== Running Periodic Scheduling Demo ===\n"
	./demo_periodic

run_pip: demo_priority_inversion
	@echo "\n=== Running Priority Inversion Demo ===\n"
	./demo_priority_inversion

run_overload: demo_overload
	@echo "\n=== Running Overload / Deadline Miss Demo ===\n"
	./demo_overload

clean:
	rm -f $(OBJ) $(DEMOS)