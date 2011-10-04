CROSS=arm-linux-
CC=$(CROSS)gcc
AR=$(CROSS)ar
LD=$(CROSS)ld
CFLAGS=-c -Wall
LDFLAGS=-lpthread
SOURCES=libavr.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=libavr.a

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(AR) -rcs $@ $(OBJECTS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
