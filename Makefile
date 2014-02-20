COMMON_OBJECTS=byte_utils.o database.o minipro.o
OBJECTS=$(COMMON_OBJECTS) main.o minipro-query-db.o
PROGS=minipro minipro-query-db
MINIPRO=minipro
MINIPRO_QUERY_DB=minipro-query-db
TESTS=$(wildcard tests/test_*.c);
OBJCOPY=objcopy

override CFLAGS += `pkg-config --cflags libusb-1.0` -g -O0
LIBS = `pkg-config --libs libusb-1.0`

all: $(OBJECTS) $(PROGS)

minipro: $(COMMON_OBJECTS) main.o
	$(CC) $(COMMON_OBJECTS) main.o $(LIBS) -o $(MINIPRO)

minipro-query-db: $(COMMON_OBJECTS) minipro-query-db.o
	$(CC) $(COMMON_OBJECTS) minipro-query-db.o $(LIBS) -o $(MINIPRO_QUERY_DB)

clean:
	rm -f $(OBJECTS) $(PROGS)

test: $(TESTS:.c=.stamp)
	$(CC) -c -I. $< $(CFLAGS) -DTEST -o $(<:.c=.o)
	$(OBJCOPY) --weaken $(<:.c=.o)
	$(CC) $(LIBS) $(COMMON_OBJECTS) $(<:.c=.o) -o $(<:.c=)
