TARGET=58g_hid
CROSS_COMPILE?=
all: $(TARGET)

$(TARGET): hid.c main.c log.c menu_58g.c stdstring.c serial.c aw5808.c
	$(CROSS_COMPILE)gcc $^ -o $@ -lreadline

clean:
	rm -f ./58g_hid