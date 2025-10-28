CFLAGS:=-Wall -Wextra -Werror
PREFIX:=/usr
build:
	$(CC) $(CFLAGS) main.c -o main `pkg-config --cflags --libs libevdev`

install:
	install -Dm644 $(DESTDIR)/$(PREFIX)/libexec/
	install main $(DESTDIR)/$(PREFIX)/libexec/mouse-emu

clean:
	rm -f main
