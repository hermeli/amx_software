CROSS=arm-linux-
CC=$(CROSS)gcc
AR=$(CROSS)ar
LD=$(CROSS)ld
CFLAGS=-c -Wall
#LDFLAGS=-lpthread -L../libavr -lavr
LDFLAGS=-lpthread -lrt
LIBRARIES=../libavr/libavr.a
SOURCES=avr-test.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=avr-test

all: $(SOURCES) $(EXECUTABLE)
	scp $(EXECUTABLE) root@10.2.24.95:~/
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBRARIES) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
