/* vi: set sw=2 ts=2 tw=80: */
/******************************************************************************
 * reader-test.c
 *
 * Testprogram for Access Manager LEGIC/MIFARE reader  driver
 *
 * Copyright (C) 2012 KABA AG, MIC AWM
 *
 * Distributed under the terms of the GNU General Public License
 * This software may be used without warrany provided and
 * copyright statements are left intact.
 * Developer: Stefan Wyss - stefan.wyss@kaba.com
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
#include <sys/time.h>
#include <linux/types.h>
#include <pthread.h>
#include "reader-test.h"
#include "spidev.h"

static void pabort(const char *s)
{
	perror(s);
	abort();
}

/*-----------------------------------------------------------------------------
 * static vars 
 *---------------------------------------------------------------------------*/
static unsigned char tx[64];				// SPI transfer buffer
static unsigned char rx[64];				// SPI receive buffer

static int spidev = 0;
static int amxdev = 0;

static struct spi_ioc_transfer tr = {		// SPI data transfer structure
		.tx_buf = 0,
		.rx_buf = 0,
		.len = 1,
		.delay_usecs = 0,
		.speed_hz = 1000000,
		.bits_per_word = 8,
};

static uint8_t mode = SPI_MODE_3;
static uint8_t bits = 8;
static uint32_t speed = 1000000;

static DRVMSG drvMsg;						// AMX control structure

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
 * calcCRCLegic() 
 *
 * calculate CRC
 *---------------------------------------------------------------------------*/
//*** CRC calculation ***

#define PRESET_VALUE 0xFFFF
#define POLYNOMIAL 0x8408

unsigned int calcCRCLegic(unsigned char data[])
{
	unsigned int crc = PRESET_VALUE;
	int i,j;
	unsigned char number_of_bytes = data[0] - 2; // in a LEGIC command the
	// first byte of a array/
	// string holds the number
	// of following bytes
	for(i = 0; i <= number_of_bytes; i++) // loop over all the bytes
	{ // which are used to build the crc
		crc = crc ^((data[i])); // xor the crc with the next byte
		for(j = 0; j < 8; ++j) // loop over each bit
		{
			if( crc & 0x0001) // if the last bit of the crc is 1
				crc = (crc >> 1)^POLYNOMIAL; // then shift the crc and xor the
			// crc with the polynomial
			else // else shift the crc
				crc = (crc >> 1);
		}
	}
	return crc;
}
/*-----------------------------------------------------------------------------
 * transferToLegic() 
 *
 *---------------------------------------------------------------------------*/
int transferToLegic()
{
	int ret;
	unsigned short crc;
	
	crc=calcCRCLegic(tx);    
	tx[tx[0]-1]=(unsigned char)(crc>>8);
	tx[tx[0]]=(unsigned char)crc;
    tr.len = tx[0]+1; 
	
	// send the txbuf message (and receive into rxbuf)
	ret = ioctl(spidev, SPI_IOC_MESSAGE(1), &tr);
	if (ret != tr.len)
	{
		printf("transferToLegic: SPI_IOC_MESSAGE failed(1) (ret=%d).\n",ret);
		return 0;
	}	
	
	// wait for txready == 1
	drvMsg.txrdy = 0;
	do 
	{
		if (ioctl(amxdev, IOCTL_AMX_GET, &drvMsg) != 0)
		{
			printf("transferToLegic: IOCTL_AMX_GET failed\n");
			return -1;
		}
	}
	while (drvMsg.txrdy == 0);
	
	// get the rxbuf message len
	tr.len = 1;
	tx[0]=0;
	ret = ioctl(spidev, SPI_IOC_MESSAGE(1), &tr);
	if (ret != tr.len)
	{
		printf("transferToLegic: SPI_IOC_MESSAGE failed(2) (ret=%d).\n",ret);
		return 0;
	}	
		
	// get the rest of the message
	tr.len = rx[0];
	tr.rx_buf = (unsigned long)(&(rx[1]));
	memset(tx,0,rx[0]);
	
	ret = ioctl(spidev, SPI_IOC_MESSAGE(1), &tr);
	if (ret != tr.len)
	{
		printf("transferToLegic: SPI_IOC_MESSAGE failed(3) (ret=%d).\n",ret);
		return 0;
	}	
	
	tr.rx_buf = (unsigned long)(&(rx[0]));
	
	crc=calcCRCLegic(rx);    
	if ( (rx[rx[0]-1]==(unsigned char)(crc>>8)) && (rx[rx[0]]==(unsigned char)crc) )
	{
		return 1;
	}
	else
	{
		printf("SPI error: bad CRC in answer\n");	
		return 0;
	}	
}
/*-----------------------------------------------------------------------------
 * main program function 
 *---------------------------------------------------------------------------*/
int main(void)
{
	struct timeval tstart, tend;
	uint delta_us;
	int i=0;
	
	char spidev_devname[20] =  "/dev/spidev1.0";
	char amx_devname[20] = "/dev/amx";

	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;

	printf("Simple Reader driver test program\r\n");

	// *** open the device nodes ***
	spidev = open((char *)spidev_devname,O_RDWR);
	if(spidev < 0) {
		printf("Error opening %s\n",spidev_devname);
		return -1;
	}
	
	amxdev = open((char *)amx_devname,O_RDWR);
	if(amxdev < 0) {
		printf("Error opening %s\n",amx_devname);
		return -1;
	}

	// *** setup SPI  ***
	setup_spi();
	
	// set legic reset 
	drvMsg.nres = LOW;
	if (ioctl(amxdev, IOCTL_AMX_SET, &drvMsg) != 0)
	{
		printf("set legic reset - IOCTL_AMX_SET failed!\n");
		return -1;
	}
	usleep(2);
	
	// release legic reset 
	drvMsg.nres = HIGH;
	if (ioctl(amxdev, IOCTL_AMX_SET, &drvMsg) != 0)
	{
		printf("release legic reset - IOCTL_AMX_SET failed!\n");
		return -1;
	}
	
	// report legic reset state 
	if (ioctl(amxdev, IOCTL_AMX_GET, &drvMsg) != 0)
	{
		printf("IOCTL_AMX_GET failed\n");
		return -1;
	}
	printf("Board type: %s\n",(drvMsg.board==AMM)?"AM-M":"AM-L");
	printf("Reset* signal state (high) = %d\n",drvMsg.nres);
	printf("Waiting 3.3s (SM4200 startup)\n");
	usleep(3300000);
	
	gettimeofday(&tstart, NULL);
	
	while (i++<1000)
	{
		// *** Read version from Legic Chip ***
		tx[0]=0x04;	// msg length
		tx[1]=0x0A;	// command
		tx[2]=0x00;	// tag
		if (!transferToLegic())
		{
			printf("TransferToLegic failed on %s\n",spidev_devname);
			return -1;
		}
	}
	gettimeofday(&tend, NULL);
	
	printf("Product Type:\t\t%s\r\n",(rx[5]==1)?"SM4200":"n.A.--");
	printf("HW-Version:\t\t%s\r\n",(rx[6]==0x05)?"1.0":"n.A.");
	
	delta_us = (uint)((tend.tv_sec-tstart.tv_sec)*1000000+tend.tv_usec-tstart.tv_usec);
	printf("Performance: %d milliseconds per SPI transfer\n",delta_us/1000000);
		
	close(spidev);
	close(amxdev);
	return 0;
}
