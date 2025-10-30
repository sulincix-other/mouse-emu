CFLAGS:=-Wall -Wextra -Werror
PREFIX:=/usr
build:
	$(CC) $(CFLAGS) main.c -o main

install:
	install -Dm644 $(DESTDIR)/$(PREFIX)/libexec/
	install main $(DESTDIR)/$(PREFIX)/libexec/mouse-emu

clean:
	rm -f main
