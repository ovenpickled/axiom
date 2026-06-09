PREFIX = /usr/local

axiom: axiom.c
	$(CC) axiom.c -o axiom -Wall -Wextra -pedantic -std=c99

install: axiom
	mkdir -p $(PREFIX)/bin
	cp axiom $(PREFIX)/bin/axiom
	chmod 755 $(PREFIX)/bin/axiom

uninstall:
	rm -f $(PREFIX)/bin/axiom

clean:
	rm -f axiom

.PHONY: install uninstall clean
