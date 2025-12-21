#For ARM64 cross-compilation add 'CROSS_COMPILER = aarch64-none-linux-gnu-' when making writer.

TARGET ?= aesdsocket
SRCS ?= aesdsocket.c
HFILES ?= aesdsocket.h
CC ?= $(CROSS_COMPILER)gcc
LDFLAGS ?= -lc -lpthread
CFLAGS ?= -g -Wall -Werror
OBJS ?= $(SRCS:.c=.o)

all: $(TARGET)

%.o:%.c $(HFILES)
	$(CC) $(CFLAGS) -c $(SRCS) -o $(TARGET).o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(TARGET)


clean:
	rm -rf $(TARGET) $(OBJS)
