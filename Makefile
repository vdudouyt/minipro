.PHONY: all clean test distclean dist \
	install install-common install-udev-rule install-bash-completion \
	uninstall uninstall-common uninstall-udev-rule uninstall-bash-completion

COMMON_OBJECTS=byte_utils.o database.o minipro.o fuses.o easyconfig.o
OBJECTS=$(COMMON_OBJECTS) main.o minipro-query-db.o
PROGS=minipro minipro-query-db
MINIPRO=minipro
MINIPRO_QUERY_DB=minipro-query-db
MINIPROHEX=miniprohex
TESTS=$(wildcard tests/test_*.c);
OBJCOPY=objcopy
VERSION=0.1

PREFIX=/usr/local

DIST_DIR = $(MINIPRO)-$(VERSION)
BIN_INSTDIR=$(DESTDIR)$(PREFIX)/bin
MAN_INSTDIR=$(DESTDIR)$(PREFIX)/share/man/man1

UDEV_DIR=$(shell pkg-config --silence-errors --variable=udevdir udev)
UDEV_RULES_INSTDIR=$(DESTDIR)$(UDEV_DIR)/rules.d

COMPLETIONS_DIR=$(shell pkg-config --silence-errors --variable=completionsdir bash-completion)
COMPLETIONS_INSTDIR=$(DESTDIR)$(COMPLETIONS_DIR)

libusb_CFLAGS = $(shell pkg-config --cflags libusb-1.0)
libusb_LIBS = $(shell pkg-config --libs libusb-1.0)

CFLAGS = -g -O0
override CFLAGS += $(libusb_CFLAGS)
override LIBS += $(libusb_LIBS)

all: version $(OBJECTS) $(PROGS)

version:
	@echo "#define VERSION \"$(VERSION)\"" > version.h

minipro: $(COMMON_OBJECTS) main.o
	$(CC) $(COMMON_OBJECTS) main.o $(LIBS) -o $(MINIPRO)

minipro-query-db: $(COMMON_OBJECTS) minipro-query-db.o
	$(CC) $(COMMON_OBJECTS) minipro-query-db.o $(LIBS) -o $(MINIPRO_QUERY_DB)

clean:
	rm -f $(OBJECTS) $(PROGS) version.h

distclean: clean
	rm -rf $(DIST_DIR)
	rm -f $(DIST_DIR).tar.gz

install-udev-rule:
	@if [ -n "$(UDEV_DIR)" -a \( ! -e "$(UDEV_RULES_INSTDIR)" -o -w "$(UDEV_RULES_INSTDIR)" \) ]; then \
		echo "Installing udev rules file"; \
		mkdir -p $(UDEV_RULES_INSTDIR); \
		cp udev/rules.d/80-minipro.rules $(UDEV_RULES_INSTDIR)/; \
	else \
		echo "NOT installing udev rules file"; \
	fi

uninstall-udev-rule:
	@if [ -n "$(UDEV_DIR)" -a -w "$(UDEV_RULES_INSTDIR)" ]; then \
		echo "Removing udev rules file"; \
		rm -f $(UDEV_RULES_INSTDIR)/80-minipro.rules; \
	else \
		echo "NOT removing udev rules file"; \
	fi

install-bash-completion:
	@if [ -n "$(COMPLETIONS_DIR)" -a \( ! -e "$(COMPLETIONS_INSTDIR)" -o -w "$(COMPLETIONS_INSTDIR)" \) ]; then \
		echo "Installing bash completion config"; \
		mkdir -p $(COMPLETIONS_INSTDIR); \
		cp bash_completion.d/minipro $(COMPLETIONS_INSTDIR)/; \
	else \
		echo "NOT installing bash completion config"; \
	fi

uninstall-bash-completion:
	@if [ -n "$(COMPLETIONS_DIR)" -a -w "$(COMPLETIONS_INSTDIR)" ]; then \
		echo "Removing bash completion config"; \
		rm -f $(COMPLETIONS_INSTDIR)/minipro; \
	else \
		echo "NOT removing bash completion config"; \
	fi

install-common:
	mkdir -p $(BIN_INSTDIR)
	mkdir -p $(MAN_INSTDIR)
	cp $(MINIPRO) $(BIN_INSTDIR)/
	cp $(MINIPRO_QUERY_DB) $(BIN_INSTDIR)/
	cp $(MINIPROHEX) $(BIN_INSTDIR)/
	cp man/minipro.1 $(MAN_INSTDIR)/

install: install-common install-udev-rule install-bash-completion

uninstall-common:
	rm -f $(BIN_INSTDIR)/$(MINIPRO)
	rm -f $(BIN_INSTDIR)/$(MINIPRO_QUERY_DB)
	rm -f $(BIN_INSTDIR)/$(MINIPROHEX)
	rm -f $(MAN_INSTDIR)/minipro.1

uninstall: uninstall-common uninstall-udev-rule uninstall-bash-completion

dist: distclean
	mkdir $(DIST_DIR)
	@for file in `ls`; do \
		if test $$file != $(DIST_DIR); then \
			cp -Rp $$file $(DIST_DIR)/$$file; \
		fi; \
	done
	find $(DIST_DIR) -type l -exec rm -f {} \;
	tar chof $(DIST_DIR).tar $(DIST_DIR)
	gzip -f --best $(DIST_DIR).tar
	rm -rf $(DIST_DIR)
	@echo
	@echo "$(DIST_DIR).tar.gz created"
	@echo
