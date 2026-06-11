PREFIX = /usr/local
VERSION ?= dev
EXTRA_FLAGS ?=
MANDIR = $(PREFIX)/share/man/man1

axiom: axiom.c
	$(CC) axiom.c -o axiom \
		-Wall -Wextra -pedantic -std=c99 \
		-DAXIOM_VERSION=\"$(VERSION)\" \
		$(EXTRA_FLAGS)

install: axiom
	mkdir -p $(PREFIX)/bin
	cp axiom $(PREFIX)/bin/axiom
	chmod 755 $(PREFIX)/bin/axiom
	mkdir -p $(MANDIR)
	gzip -c axiom.1 > $(MANDIR)/axiom.1.gz

uninstall:
	rm -f $(PREFIX)/bin/axiom
	rm -f $(MANDIR)/axiom.1.gz

clean:
	rm -f axiom

.PHONY: install uninstall clean
