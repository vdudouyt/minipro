.PHONY: all clean install test
COMMON_OBJECTS=byte_utils.o database.o minipro.o fuses.o easyconfig.o
OBJECTS=$(COMMON_OBJECTS) main.o minipro-query-db.o
PROGS=minipro minipro-query-db
MINIPRO=minipro
MINIPRO_QUERY_DB=minipro-query-db
TESTS=$(wildcard tests/test_*.c);
OBJCOPY=objcopy

BIN_DIR=$(DESTDIR)/usr/bin/
UDEV_RULES_DIR=$(DESTDIR)/etc/udev/rules.d/
MAN_DIR=$(DESTDIR)/usr/share/man/man1/
COMPLETIONS_DIR=$(DESTDIR)/etc/bash_completion.d/

override CFLAGS += `pkg-config --cflags libusb-1.0` -g -O0
LIBS = `pkg-config --libs libusb-1.0`

all: $(OBJECTS) $(PROGS)

minipro: $(COMMON_OBJECTS) main.o
	$(CC) $(COMMON_OBJECTS) main.o $(LIBS) -o $(MINIPRO)

minipro-query-db: $(COMMON_OBJECTS) minipro-query-db.o
	$(CC) $(COMMON_OBJECTS) minipro-query-db.o $(LIBS) -o $(MINIPRO_QUERY_DB)

clean:
	rm -f $(OBJECTS) $(PROGS)

install:
	mkdir -p $(BIN_DIR)
	mkdir -p $(UDEV_RULES_DIR)
	mkdir -p $(MAN_DIR)
	mkdir -p $(COMPLETIONS_DIR)
	cp $(MINIPRO) $(BIN_DIR)
	cp $(MINIPRO_QUERY_DB) $(BIN_DIR)
	cp udev/rules.d/80-minipro.rules $(UDEV_RULES_DIR)
	cp bash_completion.d/minipro $(COMPLETIONS_DIR)
	cp man/minipro.1 $(MAN_DIR)
