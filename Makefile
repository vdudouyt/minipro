SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
PROGNAME=minipro
TESTS=$(wildcard tests/test_*.c);
OBJCOPY=objcopy

CFLAGS = `pkg-config --cflags libusb-1.0` -g -O0
LIBS = `pkg-config --libs libusb-1.0`

all: $(OBJECTS)
	$(CC) $(LIBS) $(OBJECTS) -o $(PROGNAME)

clean:
	rm -f $(OBJECTS) $(PROGNAME)

test: $(TESTS:.c=.stamp)
	$(CC) -c -I. $< $(CFLAGS) -DTEST -o $(<:.c=.o)
	$(OBJCOPY) --weaken $(<:.c=.o)
	$(CC) $(LIBS) $(OBJECTS) $(<:.c=.o) -o $(<:.c=)
