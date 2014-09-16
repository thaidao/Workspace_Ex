VERSION = 1
PATCHLEVEL = 0
SUBLEVEL = 0
EXTRAVERSION = .git
NAME = tablet_rosa
CC=arm-linux-gnueabi-gcc
CFLAGS=-O2 -Wall

override CFLAGS += -DVERSION=$(VERSION)
override CFLAGS += -DPATCHLEVEL=$(PATCHLEVEL)
override CFLAGS += -DSUBLEVEL=$(SUBLEVEL)
override CFLAGS += -DVERSION_NAME=\"$(NAME)\"

PROGS=perf rect fb-test offset fb-string

all: $(PROGS)

.c.o: common.h font.h

fb-test: fb-test.o common.o font_8x8.c

fb-string: fb-string.o common.o font_8x8.c

clean:
	rm -f $(PROGS) *.o
