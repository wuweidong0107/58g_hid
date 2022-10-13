TARGET=aw5808
CROSS_COMPILE?=
all: $(TARGET)

$(TARGET): hid.c main.c log.c menu.c stdstring.c serial.c aw5808.c device.c thpool.c
	$(CROSS_COMPILE)gcc $^ -o $@ -lreadline -lpthread -lev

clean:
	rm -f ./aw5808