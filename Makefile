CC ?= gcc
CFLAGS ?= -O3 -Wall -Wextra -pedantic
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
TARGET = seeker
ALIAS = seek
VERSION = 1.0.0

all: $(TARGET)

$(TARGET): seek.c
	$(CC) $(CFLAGS) seek.c -o $(TARGET)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	ln -sf $(TARGET) $(DESTDIR)$(BINDIR)/$(ALIAS)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(BINDIR)/$(ALIAS)

clean:
	rm -f $(TARGET) $(ALIAS) seek *.deb *.tar.gz *.tar.zst
	rm -rf dist packaging/deb/pkg

# Build Debian / Ubuntu .deb package
deb:
	@chmod +x packaging/debian/build-deb.sh
	@./packaging/debian/build-deb.sh

# Build Arch Linux package with makepkg
arch:
	@cd packaging/arch && makepkg -si

.PHONY: all install uninstall clean deb arch
