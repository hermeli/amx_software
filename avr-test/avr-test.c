#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>

#define DEVICE "/dev/ttyS4"

static int avr;

void *read_from_avr (void *args)
{
	uint8_t buf[4];
	int count;

	printf("start reading from avr\n");

	while(1)
	{
		count = read(avr, buf, 2);
	
		if(count > 0)
		{
			printf("received 0x%x%02x,\n", buf[1], buf[0]);
		}
	}

	return NULL;
}

int open_port(void){
	int fd_ser;
	struct termios terminal;
	fd_ser = open(DEVICE, O_RDWR | O_NOCTTY);

	if (fd_ser == -1)
	{
		printf("Error opening %s\n", DEVICE);
		return -1;
	}

	tcgetattr(fd_ser, &terminal);
	
	//
	// No line processing:
	// canonical mode off
	//
	terminal.c_lflag &= ~(ICANON);

	tcflush(fd_ser,TCIOFLUSH);
	tcsetattr(fd_ser,TCSANOW,&terminal);
	return fd_ser;
}

int main(void)
{
	uint8_t reset_off[4] = { 0x87, 0x01, 0x0E, 0x01 };
	uint8_t reset_all[4] = { 0x85, 0x01, 0x10, 0x01 };

	pthread_t read_thread;

	printf("Simple AVR test program\n");

	// *** open the device nodes ***
	avr = open_port();

	if(avr < 0) {
		return -1;
	}

	pthread_create (&read_thread, NULL, read_from_avr, NULL);

	write(avr, reset_off, 4);

	sleep(2);

	write(avr, reset_all, 4);

	sleep(2);

	pthread_join (read_thread, NULL);

	close(avr);

	return 0;
}
