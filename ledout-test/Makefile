TARGET=ledout-test

#CROSS=arm-linux-
CROSS=arm-none-linux-gnueabi-
CC=$(CROSS)gcc
AR=$(CROSS)ar
LD=$(CROSS)ld
CFLAGS=-c -Wall
LDFLAGS=-static -lpthread
SOURCES=$(TARGET).c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=$(TARGET)
# LD_LIBRARY_PATH=/home/llandis/amm/amx-buildroot/output/toolchain/uClibc-0.9.32/lib

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
