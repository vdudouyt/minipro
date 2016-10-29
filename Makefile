.PHONY: all clean install test
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
BIN_DIR=$(DESTDIR)$(PREFIX)/bin/
UDEV_RULES_DIR=$(DESTDIR)$(PREFIX)/lib/udev/rules.d/
MAN_DIR=$(DESTDIR)$(PREFIX)/share/man/man1/
COMPLETIONS_DIR=$(DESTDIR)$(PREFIX)/share/bash_completion.d/

libusb_CFLAGS = `pkg-config --cflags libusb-1.0`
libusb_LIBS = `pkg-config --libs libusb-1.0`

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

install:
	mkdir -p $(BIN_DIR)
	mkdir -p $(UDEV_RULES_DIR)
	mkdir -p $(MAN_DIR)
	mkdir -p $(COMPLETIONS_DIR)
	cp $(MINIPRO) $(BIN_DIR)
	cp $(MINIPRO_QUERY_DB) $(BIN_DIR)
	cp $(MINIPROHEX) $(BIN_DIR)
	cp udev/rules.d/80-minipro.rules $(UDEV_RULES_DIR)
	cp bash_completion.d/minipro $(COMPLETIONS_DIR)
	cp man/minipro.1 $(MAN_DIR)

uninstall:
	rm -f $(BIN_DIR)/$(MINIPRO)
	rm -f $(BIN_DIR)/$(MINIPRO_QUERY_DB)
	rm -f $(BIN_DIR)/$(MINIPROHEX)
	rm -f $(UDEV_RULES_DIR)/80-minipro.rules
	rm -f $(COMPLETIONS_DIR)/minipro
	rm -f $(MAN_DIR)/minipro.1
	find $(BIN_DIR) -type d -empty -delete
	find $(COMPLETIONS_DIR) -type d -empty -delete
	find $(MAN_DIR) -type d -empty -delete

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
	
