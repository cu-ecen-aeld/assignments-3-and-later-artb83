#For ARM64 cross-compilation add 'CROSS_COMPILER = aarch64-none-linux-gnu-' when making writer.

TARGET=aesdsocket
SRCS=aesdsocket.c
HFILES=aesdsocket.h
CC=gcc
LIBS=-lc -lpthread
CFLAGS=-g -Wall -Werror
OBJS=$(SRCS:.c=.o)

all: $(TARGET)

%.o:%.c $(HFILES)
	$(CROSS_COMPILER)$(CC) $(CFLAGS) -c $(SRCS) -o $(TARGET).o

$(TARGET): $(OBJS)
	$(CROSS_COMPILER)$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $(TARGET)


clean:
	rm -rf $(TARGET) $(OBJS)
