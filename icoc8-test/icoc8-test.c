/* vi: set sw=2 ts=2 tw=80: */
/******************************************************************************
 * icoc8_test.c
 *
 * Testprogram for Access Manager MIFARE IC8 & OC8 support driver
 *
 * Copyright (C) 2011 KABA AG, CC EAC
 *
 * Distributed under the terms of the GNU General Public License
 * This software may be used without warrany provided and
 * copyright statements are left intact.
 * Developer: Stefan Wyss - swyss@kbr.kaba.com
 *
 *****************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <pthread.h>
#include "icoc8.h"
#include "spidev.h"

static void pabort(const char *s)
{
	perror(s);
	abort();
}

/*-----------------------------------------------------------------------------
 * IC8 message defines
 *---------------------------------------------------------------------------*/
#define GET1					0x94
#define GET2					0x75
#define GET3					0x76
#define GET4					0x57
#define CLEARBUFFER		0x5E
#define IC8MODULE			0x99
#define RECIERR				0xF5 
#define BUFEMPTY			0x05

/*-----------------------------------------------------------------------------
 * static vars 
 *---------------------------------------------------------------------------*/
static int icoc8, spidev, nrOfOC8 = 0, nrOfIC8 = 0;

static unsigned char tx[64];						// SPI transfer buffer
static unsigned char rx[64];						// SPI receive buffer

static struct spi_ioc_transfer tr = {		// SPI data tranfer structure
		.tx_buf = 0,
		.rx_buf = 0,
		.len = 1,
		.delay_usecs = 0,
		.speed_hz = 500000,
		.bits_per_word = 8,
};

static DRVMSG drvMsg;										// SPI control structure

static uint8_t mode = SPI_MODE_3;
static uint8_t bits = 8;
static uint32_t speed = 500000;

/*-----------------------------------------------------------------------------
 * setup_spi() 
 *
 * setup SPI for normal transfer
 *---------------------------------------------------------------------------*/
void setup_spi()
{	
	int ret;

	// SPI mode
	ret = ioctl(spidev, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	// SPI bits
	ret = ioctl(spidev, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");
	
	// SPI speed
	ret = ioctl(spidev, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
}
/*-----------------------------------------------------------------------------
 * count_nr_of_oc8() 
 *
 * Clock out 0xFF into OC8 register chain until feedback (MSB=1). 
 *---------------------------------------------------------------------------*/
int count_nr_of_oc8(void)
{
	int ret;
	
	// empty the OC8 shift registers
	memset(tx,0,16);
	tr.len = 16;
	ioctl(spidev, SPI_IOC_MESSAGE(1), &tr);

	// shift 1's into registers until feedback
	for (nrOfOC8=0; nrOfOC8<16; nrOfOC8++)
	{
		// query IOBACK signal state
		ret = ioctl(icoc8, IOCTL_ICOC8_GETIOBACK, &drvMsg);
		if (ret != 0)
		{
			pabort("Can't get IOBACK signal (IOCTL_ICOC8_GETIOBACK failed)\n");
			return -1;
		}

		if (drvMsg.ioback == 1)
			break;

		// clock next OC8 byte 
		memset(tx,0xFF,1);
		tr.len = 1;
		ret = ioctl(spidev, SPI_IOC_MESSAGE(1), &tr);
		if (ret != 1)
		{
			printf("count_nr_of_oc8: SPI_IOC_MESSAGE failed (ret=%d).\n",ret);
			return -1;
		}
	}
	if (nrOfOC8 == 16) 
		nrOfOC8 = 0;
	
	printf("Detected %d OC8 modules on the bus\r\n",nrOfOC8);
	return 1;
}

/*-----------------------------------------------------------------------------
 * display_oc8_patterns() 
 *
 * Shift out a walking pattern on all connected OC8 modules. 
 *---------------------------------------------------------------------------*/
int display_oc8_patterns()
{
	int count=0, ret;
	tx[0] = 0x01;
   
	while(count<24)
  {
		// write pattern to OC8 buffer (multiple OC8 with same pattern)
		memset(tx,(0x01<<(count++%8)),nrOfOC8);
		
		tr.len = nrOfOC8;
		ret = ioctl(spidev, SPI_IOC_MESSAGE(1), &tr);
		if (ret != nrOfOC8)
		{
			printf("display_oc8_patterns: SPI_IOC_MESSAGE failed (ret=%d).\n",ret);
			return -1;
		}

		// --- strobe the OC8 outputs  
		ret = ioctl(icoc8, IOCTL_ICOC8_STROBE, NULL);	
		if (ret != 0)
		{
			pabort("can't strobe OC8");
			return -1;
		}
		usleep(200000);
	}
	return 1;
}

/*-----------------------------------------------------------------------------
 * unsigned char transfer_ic8_byte(unsigned char chipsel, unsigned char data) 
 *
 *---------------------------------------------------------------------------*/
unsigned char transfer_ic8_byte(unsigned char chipsel, unsigned char data)
{
		int ret;
		DRVMSG xctl;
		unsigned char txbuf[1];
		unsigned char rxbuf[1];
	
		struct spi_ioc_transfer xfer = {		// SPI data tranfer structure
			.tx_buf = (unsigned long)txbuf,
			.rx_buf = (unsigned long)rxbuf,
			.len = 1,
			.delay_usecs = 0,
			.speed_hz = 500000,
			.bits_per_word = 8,
		};

		// setup the chip select  
		xctl.chipsel = chipsel;
		ret = ioctl(icoc8, IOCTL_ICOC8_SETCS, &xctl);	
		if (ret != 0)
		{
			printf("transfer_ic8_byte: IOCTL_ICOC8_SETCS failed (ret=%d)\n",ret);
			return -1;
		}
		
		// send the txbuf message (and receive into rxbuf)
		txbuf[0] = data;
		ret = ioctl(spidev, SPI_IOC_MESSAGE(1), &xfer);
		if (ret != 1)
		{
			printf("transfer_ic8_byte: SPI_IOC_MESSAGE failed (ret=%d).\n",ret);
			return -1;
		}	

		// setup the chip select  
		xctl.chipsel = 0xF;
		ret = ioctl(icoc8, IOCTL_ICOC8_SETCS, &xctl);	
		if (ret != 0)
		{
			printf("transfer_ic8_byte: IOCTL_ICOC8_SETCS failed (ret=%d)\n",ret);
			return -1;
		}

		return rxbuf[0];
}

/*-----------------------------------------------------------------------------
 * void decode_ic8_answer(unsigned char data) 
 *
 * check IC8 parity and decode input
 *---------------------------------------------------------------------------*/
void decode_ic8_answer(unsigned char data) 
{
	unsigned char parity = (data >> 5);
	unsigned char bitcount = 0;
	unsigned char inputnr;
	unsigned char swopen;
	int i=0;

	for (i=0; i<5; i++)
	{
		if (data&(1<<i)) 
			bitcount++;
	}

	if ( (bitcount+1) != parity )
	{
		printf("IC8 parity error!\n");
		return;
	}
	
	inputnr = ((data >> 2) & 0x07)+1;
	swopen = (data & 0x01);

	(swopen)? printf("input %d opened\n",inputnr): printf("input %d closed\n",inputnr);

}

/*-----------------------------------------------------------------------------
 * read_ic8_thread() 
 *
 * read out the IC8 devices connected to the bus
 *---------------------------------------------------------------------------*/
void *read_ic8_thread(void *args)
{
	unsigned char ans = 0;

	if (nrOfIC8 == 0) 
		return NULL;

	while(1)
	{
get1:
		ans = transfer_ic8_byte(0, GET1);
		if (ans == RECIERR)
			goto get4;
		if (ans != BUFEMPTY)
			decode_ic8_answer(ans); 
		usleep(100000);
get2:
		ans = transfer_ic8_byte(0, GET2);
		if (ans == RECIERR)
			goto get1;
		if (ans != BUFEMPTY)
			decode_ic8_answer(ans); 
		usleep(100000);
get3:
		ans = transfer_ic8_byte(0, GET3);
		if (ans == RECIERR)
			goto get2;
		if (ans != BUFEMPTY)
			decode_ic8_answer(ans); 
		usleep(100000);
get4:
		ans = transfer_ic8_byte(0, GET4);
		if ( ans == RECIERR)
			goto get3;
		if (ans != BUFEMPTY)
			decode_ic8_answer(ans); 
		usleep(100000);
		goto get1;
	}
	return NULL;
}

/*-----------------------------------------------------------------------------
 * count_nr_of_ic8() 
 *
 * count up CS lines and query SPI bus to see how many IC8 reply.
 *---------------------------------------------------------------------------*/
int count_nr_of_ic8(void)
{
	int ret, i;

	// detect number of IC8
	for (i=0; i<15; i++)
	{	
		// setup the chip select  
		drvMsg.chipsel = i;
		ret = ioctl(icoc8, IOCTL_ICOC8_SETCS, &drvMsg);	
		if (ret != 0)
		{
			printf("count_nr_of_ic8: IOCTL_ICOC8_SETCS failed (ret=%d)\n",ret);
			return -1;
		}

		// send the CLEARBUFFER message
		tx[0] = CLEARBUFFER;
		tr.len = 1;
		ret = ioctl(spidev, SPI_IOC_MESSAGE(1), &tr);
		if (ret != 1)
		{
			printf("count_nr_of_ic8: SPI_IOC_MESSAGE failed (ret=%d).\n",ret);
			return -1;
		}	

		// clock back the answer byte
		tx[0] = 0;
		tr.len = 1;
		ret = ioctl(spidev, SPI_IOC_MESSAGE(1), &tr);
		if (ret != 1)
		{
			printf("count_nr_of_ic8: SPI_IOC_MESSAGE failed (ret=%d).\n",ret);
			return -1;
		}	
	
		// reset the chip select  
		drvMsg.chipsel = 0xF;
		ret = ioctl(icoc8, IOCTL_ICOC8_SETCS, &drvMsg);	
		if (ret != 0)
		{
			printf("count_nr_of_ic8: IOCTL_ICOC8_SETCS failed (ret=%d)\n",ret);
			return -1;
		}
		
		// update IC8 counter
		if (rx[0]==IC8MODULE)
			nrOfIC8++;
		else {
			break;
		}
	}
	
	printf("Detected %d IC8 modules on the bus\r\n",nrOfIC8);
	return 1;
}
/*-----------------------------------------------------------------------------
 * main program function 
 *---------------------------------------------------------------------------*/
int main(void)
{
	pthread_t ic8_thread;

	char icoc8_devname[20] = "/dev/icoc8";
	char spidev_devname[20] =  "/dev/spidev0.1";

	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;

	printf("Simple ICOC8 driver test program\r\n");

	// *** open the device nodes ***
	icoc8 = open((char *)icoc8_devname,O_RDWR);
	if(icoc8 < 0) {
		printf("Error opening %s\n",icoc8_devname);
		return -1;
	}

	spidev = open((char *)spidev_devname,O_RDWR);
	if(spidev < 0) {
		printf("Error opening %s\n",spidev_devname);
		return -1;
	}

	// *** setup SPI ***
	setup_spi();

	// *** count number of OC8 modules on SPI bus ***
	if (!count_nr_of_oc8()) 
		return -1;

	// *** count number of IC8 modules on SPI bus ***
	if (!count_nr_of_ic8())
		return -1;

	// *** display IC8 inputs on OC8 outputs
	pthread_create(&ic8_thread, NULL, read_ic8_thread, NULL);

	// ***  display LED pattern on OC8 modules ***
	if (!display_oc8_patterns())
		return -1;

	close(spidev);
	close(icoc8);
	return 0;
}
