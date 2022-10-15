
CROSS_COMPILE ?=
AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm

STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump

export AS LD CC CPP AR NM
export STRIP OBJCOPY OBJDUMP

CFLAGS := -Wall -O2 -g
CFLAGS += -I $(shell pwd)/

LDFLAGS := -lreadline -lpthread -lev

export CFLAGS LDFLAGS

TOPDIR := $(shell pwd)
export TOPDIR

TARGET := aw5808

obj-y += main.o
obj-y += menu.o
obj-y += log.o
obj-y += stdstring.o
obj-y += iobuf.o
obj-y += thpool.o
obj-y += device.o
obj-y += stdstring.o
obj-y += aw5808.o
obj-y += hid.o
obj-y += serial.o

all : 
	make -C ./ -f $(TOPDIR)/Makefile.build
	$(CC) -o $(TARGET) built-in.o $(LDFLAGS)

clean:
	rm -f $(shell find -name "*.o")
	rm -f $(shell find -name "*.d")
	rm -f $(TARGET)
	