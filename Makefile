CC ?= gcc
CFLAGS ?= -O3 -Wall -Wextra -pedantic
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
TARGETS = seeker seekui
ALIAS = seek
VERSION = 1.0.0

all: $(TARGETS)

seeker: seek.c
	$(CC) $(CFLAGS) seek.c -o seeker

seekui: seekui.c
	$(CC) $(CFLAGS) seekui.c -o seekui

install: $(TARGETS)
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 seeker $(DESTDIR)$(BINDIR)/seeker
	ln -sf seeker $(DESTDIR)$(BINDIR)/$(ALIAS)
	install -m 755 seekui $(DESTDIR)$(BINDIR)/seekui

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/seeker
	rm -f $(DESTDIR)$(BINDIR)/$(ALIAS)
	rm -f $(DESTDIR)$(BINDIR)/seekui

clean:
	rm -f $(TARGETS) $(ALIAS) seek *.deb *.tar.gz *.tar.zst
	rm -rf dist packaging/deb/pkg

deb:
	@chmod +x packaging/debian/build-deb.sh
	@./packaging/debian/build-deb.sh

arch:
	@cd packaging/arch && makepkg -si

.PHONY: all install uninstall clean deb arch
