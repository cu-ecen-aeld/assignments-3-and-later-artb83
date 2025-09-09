#For ARM64 cross-compilation add 'CROSS_COMPILER = aarch64-none-linux-gnu-' when making writer.

TARGET = writer
SRCS = writer.c
CC = gcc
CFLAGS = -g -Wall
LIBS = -lc


OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CROSS_COMPILER)$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

%.o: %.c
	$(CROSS_COMPILER)$(CC) -o $(TARGET) $(CFLAGS) -c $< -o $@
		
clean:
	rm -f $(OBJS) $(TARGET)
