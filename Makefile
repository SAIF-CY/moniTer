# Fallback Makefile (use CMake when available)
CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?= -pthread

SRCS = src/main.c src/cpu.c src/memory.c src/gpu.c src/network.c \
       src/disk.c src/sensors.c src/process.c src/data.c src/ui.c src/util.c

OBJS = $(SRCS:.c=.o)
TARGET = bios-monitor

NCURSES_CFLAGS := $(shell pkg-config --cflags ncursesw 2>/dev/null || pkg-config --cflags ncurses 2>/dev/null || echo -D_XOPEN_SOURCE=600)
NCURSES_LIBS   := $(shell pkg-config --libs ncursesw 2>/dev/null || pkg-config --libs ncurses 2>/dev/null || echo -lncursesw)

CFLAGS  += -Iinclude $(NCURSES_CFLAGS)
LDFLAGS += $(NCURSES_LIBS) -lm

# Optional libsensors
ifneq ($(shell pkg-config --exists libsensors 2>/dev/null && echo yes),)
CFLAGS  += -DHAVE_SENSORS=1 $(shell pkg-config --cflags libsensors)
LDFLAGS += $(shell pkg-config --libs libsensors)
endif

# Optional NVML
ifneq ($(wildcard /usr/lib/x86_64-linux-gnu/libnvidia-ml.so*),)
CFLAGS  += -DHAVE_NVML=1
LDFLAGS += -lnvidia-ml
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)