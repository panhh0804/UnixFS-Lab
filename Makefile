CC = gcc
CFLAGS = -Wall -g -std=c99
TARGET = fs
LDLIBS =
ifeq ($(OS),Windows_NT)
TARGET = fs.exe
LDLIBS = -lws2_32
endif
SRCS = main.c igetput.c iallfre.c ballfre.c rdwt.c creat.c open.c close.c \
       delete.c dir.c name.c access.c log.c install.c format.c halt.c \
       chmod.c link.c mv.c stat.c wc.c find.c chown.c lseek.c superblock.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c filesys.h superblock.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) filesystem
