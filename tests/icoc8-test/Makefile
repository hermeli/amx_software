CROSS=arm-linux-
CC=$(CROSS)gcc
AR=$(CROSS)ar
LD=$(CROSS)ld
CFLAGS=-c -Wall
LDFLAGS=-lpthread
SOURCES=icoc8-test.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=icoc8-test.bin

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	scp $(EXECUTABLE) 10.2.24.73:~

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
