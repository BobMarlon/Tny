CC=gcc
CFLAGS=-c -Wall -std=c99 -O2
SOURCES=src/tests.c src/tny/tny.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=bin/tny-tests

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(OBJECTS)
	rm -rf $(EXECUTABLE)
