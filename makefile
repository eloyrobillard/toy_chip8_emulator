CFLAGS=-g
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
LDFLAGS=-lraylib -lm -lpthread -ldl -lrt -lX11  -g

test: chip8
	./chip8

chip8: $(OBJS)
	$(CC) $(CFLAGS) -o chip8  $(OBJS) $(LDFLAGS)

$(OBJS): chip8.h

clean: 
	rm -f chip8 *.o 

.PHONY: test clean
