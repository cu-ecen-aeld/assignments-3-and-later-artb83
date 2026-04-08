#For ARM64 cross-compilation add 'CROSS_COMPILE = aarch64-none-linux-gnu-g' when building exec.

TARGET ?= aesd-circular-buffer
SRCS ?= aesd-circular-buffer.c
HFILES ?= aesd-circular-buffer.h
CC ?= gcc
LDFLAGS ?= -lc -lpthread
CFLAGS ?= -g -Wall -Werror
OBJS ?= $(SRCS:.c=.o)

all: $(TARGET)

%.o:%.c $(HFILES)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $(SRCS) -o $(TARGET).o

$(TARGET): $(OBJS) aesd-circular-buffer-main.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) aesd-circular-buffer-main.c -o $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJS)
