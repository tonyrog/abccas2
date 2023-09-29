CC      = gcc
CFLAGS  = -O2

OBJS = abccas2.o 

default: abccas2

abccas2: $(OBJS)
	$(CC) $(CFLAGS) -o$@ $(OBJS)

%.o:	%.c
	$(CC) -c -o $@ -MMD -MF .$<.d $(CFLAGS) $<

-include .*.d

