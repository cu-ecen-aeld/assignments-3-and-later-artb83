#For ARM64 cross-compilation add 'CROSS_COMPILE = aarch64-none-linux-gnu-g' when making writer.

TARGET ?= aesdsocket
SRCS ?= aesdsocket.c
HFILES ?= aesdsocket.h
CC ?= gcc
LDFLAGS ?= -lc -lpthread
INC_DIR ?= -I$(PWD)/../aesd-char-driver
CFLAGS += -g -Wall -Werror $(INC_DIR)
OBJS ?= $(SRCS:.c=.o)


all: $(TARGET)

%.o:%.c $(HFILES)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $(SRCS) -o $(TARGET).o

$(TARGET): $(OBJS)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(TARGET)


clean:
	rm -rf $(TARGET) $(OBJS)
