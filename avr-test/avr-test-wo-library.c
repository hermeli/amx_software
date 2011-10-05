#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <termios.h>

#define DEVICE "/dev/ttyS4"

static int avr;

void *read_from_avr (void *args)
{
	uint8_t buf[4];
	int count;

	printf("start reading from avr\n");

	while(1)
	{
		count = read(avr, buf, 4);
	
		if(count > 0)
		{
			printf("received %x, %x, %x, %x\n", buf[0], buf[1], buf[2], buf[3]);
			
			if(buf[0] == 0xa3)
				return NULL;
		}
		else
			printf("nothing received from avr\n");
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
	// Turn off character processing
	// clear current char size mask, no parity checking,
	// no output processing, force 8 bit input
	//
	terminal.c_cflag &= ~(PARENB);

	//
	// Input flags - Turn off input processing
	// convert break to null byte, no CR to NL translation,
	// no NL to CR translation, don't mark parity errors or breaks
	// no input parity check, don't strip high bit off,
	// no XON/XOFF software flow control
	//
	terminal.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
		            INLCR | PARMRK | INPCK | ISTRIP | IXON);
	//
	// Output flags - Turn off output processing
	// no CR to NL translation, no NL to CR-NL translation,
	// no NL to CR translation, no column 0 CR suppression,
	// no Ctrl-D suppression, no fill characters, no case mapping,
	// no local output processing
	//
	// config.c_oflag &= ~(OCRNL | ONLCR | ONLRET |
	//                     ONOCR | ONOEOT| OFILL | OLCUC | OPOST);
	terminal.c_oflag = 0;
	
	//
	// No line processing:
	// echo off, echo newline off, canonical mode off, 
	// extended input processing off, signal chars off
	//
	terminal.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
	
	//
	// One input byte is enough to return from read()
	// Inter-character timer off
	//
	terminal.c_cc[VMIN]  = 2;
	terminal.c_cc[VTIME] = 10;

	tcflush(fd_ser,TCIOFLUSH);
	tcsetattr(fd_ser,TCSANOW,&terminal);
	return fd_ser;
}

int main(void)
{
	uint8_t reset_off[4] = { 0x87, 0x01, 0x0E, 0x01 };	 // '\x87''\x01''\x0E''\x01'
	uint8_t reset_all[4] = { 0x85, 0x01, 0x10, 0x01 };	 // '\x87''\x01''\x0E''\x01'

	char avr_devname[11] = "/dev/ttyS4";

	pthread_t read_thread;

	printf("Simple AVR test program\n");

	// *** open the device nodes ***
	avr = open_port();

	if(avr < 0) {
		printf("Error opening %s\n",avr_devname);
		return -1;
	}

	pthread_create (&read_thread, NULL, read_from_avr, NULL);

	printf("reset_off\n");
	write(avr, reset_off, 4);

	sleep(2);

	printf("reset_all\n");
	write(avr, reset_all, 4);

	sleep(2);

	pthread_join (read_thread, NULL);

	close(avr);

	return 0;
}
