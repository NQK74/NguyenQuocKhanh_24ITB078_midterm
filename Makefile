# Works with both BSD make (NetBSD: make) and GNU make (gmake)
CC      = cc
CFLAGS  = -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -D_NETBSD_SOURCE
TARGET  = myls
OBJS    = main.o options.o entry.o sort.o print.o list.o
HDRS    = options.h entry.h sort.h print.h list.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

# Every object depends on all headers (safe, simple rebuild rule)
$(OBJS): $(HDRS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
