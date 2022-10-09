TARGET=58g_hid

all: $(TARGET)

$(TARGET): hid.c main.c log.c 58g_hid.c
	gcc $^ -o $@ -lreadline

clean:
	rm -f ./58g_hid