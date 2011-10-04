#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

static int avr;

void *read_from_avr (void *args)
{
	uint8_t buf[4];
	int count;

	printf("start reading from avr\n");

	while(1)
	{
		count = read(avr, buf, 1);
	
		if(count > 0)
		{
			printf("received %x, %x, %x, %x\n", buf[0], buf[1], buf[2], buf[3]);
			return NULL;
		}
		else
			printf("nothing received from avr\n");
	}

	return NULL;
}

int main(void)
{
	uint8_t reset_off[4] = { 0x87, 0x01, 0x0E, 0x01 };	 // '\x87''\x01''\x0E''\x01'
	uint8_t reset_all[4] = { 0x85, 0x01, 0x10, 0x01 };	 // '\x87''\x01''\x0E''\x01'

	char avr_devname[11] = "/dev/ttyS4";

	pthread_t read_thread;

	printf("Simple AVR test program\n");

	// *** open the device nodes ***
	avr = open((char *)avr_devname, O_RDWR | O_NOCTTY);

	if(avr < 0) {
		printf("Error opening %s\n",avr_devname);
		return -1;
	}

	pthread_create (&read_thread, NULL, read_from_avr, NULL);

	write(avr, reset_off, 4);

	// sleep(2);

	// write(avr, reset_all, 4);

	sleep(2);

	pthread_join (read_thread, NULL);

	close(avr);

	return 0;
}
