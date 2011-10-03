/***********************************************************************
						(C) KABA Access Management

 Driver:	avr.c

 Function:	AVR Driver - Interface for communicating (COM) with the AVR 
  			hardware.

 Autor:    	Lukas Landis (llandis@kbr.kaba.com)
 Date:   	03.10.2011

***********************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>

#include "avr.h"

#define DEVICE 					"/dev/ttyS4"
#define QUEUE_MASK				511
#define SEND_BUFFER_SIZE		512
#define TEMP_BUFFER_SIZE		16

typedef struct 
{
	uint8_t data[QUEUE_MASK + 1];
	int read;
	int write;
} queue_t;

static int avr;

static queue_t event_queue;
static queue_t answer_queue;
static queue_t data_queue;

static pthread_t read_thread;

static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t avr_mutex = PTHREAD_MUTEX_INITIALIZER;

uint8_t enqueue(uint8_t data, queue_t *queue)
{
	uint32_t next;
	
	pthread_mutex_lock(&queue_mutex);
	
	next = ((queue->write + 1) & QUEUE_MASK);
	
	if (queue->read == next)
	{
		pthread_mutex_unlock(&queue_mutex);
		return 0;
	}
	
	queue->data[queue->write] = data;
	queue->write = next;
	
	pthread_mutex_unlock(&queue_mutex);
	return 1;
}

uint8_t dequeue(uint8_t *data, queue_t *queue)
{
	pthread_mutex_lock(&queue_mutex);
	
	if (queue->read == queue->write)
	{
		pthread_mutex_unlock(&queue_mutex);
		return 0;
	}
	
	*data = queue->data[queue->read];
	queue->read = ((queue->read + 1) & QUEUE_MASK);
	
	pthread_mutex_unlock(&queue_mutex);
	return 1;
}

int count_queue(queue_t *queue)
{
	int count;
	
	pthread_mutex_lock(&queue_mutex);
	
	count = ((queue->write - queue->read) & QUEUE_MASK);
	
	pthread_mutex_unlock(&queue_mutex);
	
	return count;
}

void init_queue(queue_t *queue)
{
	pthread_mutex_lock(&queue_mutex);
	
	queue->write = 0;
	queue->read = 0;
	
	pthread_mutex_unlock(&queue_mutex);
}

int __nsleep(const struct timespec *req, struct timespec *rem)
{
    struct timespec temp_rem;
    if(nanosleep(req,rem)==-1)
        __nsleep(rem,&temp_rem);
    else
        return 1;
        
    return 0;
}
 
int msleep(unsigned long milisec)
{
    struct timespec req={0},rem={0};
    time_t sec=(int)(milisec/1000);
    milisec=milisec-(sec*1000);
    req.tv_sec=sec;
    req.tv_nsec=milisec*1000000L;
    __nsleep(&req,&rem);
    
    return 1;
}

uint8_t avr_sum(uint8_t *data, int count, uint8_t k)
{
	uint8_t sum = (*data++) - k;
	int n;

	for(n = 1; n < count; n++)
	{		
		sum = (sum & 0x80) ? (sum << 1) - (*data++) - 1: (sum << 1) - (*data++);
	}
	
	if (sum & 0x80) sum =~ sum;
	
	return sum;
}

// return: The type (bit 0 -> answer, bit 1 -> event, bit 2 -> antcom)
uint8_t calculate_right_type(uint8_t buffer)
{
	uint8_t retbit = 0;

	if (buffer & 0x40)		// Packet vollstaendig
	{
		retbit |= 0x01;
	}
	else					
	{	
		if (buffer & 0x008)
		{
			// An AntCom Interrupt event...
			if (buffer & 0x010)
			{
				retbit |= 0x04;
			}

			if (buffer & 0x020)
			{
				retbit |= 0x02;
			}
		}
		else
		{
			retbit |= 0x02;
		}
	}

	return retbit;
}

void enqueue_to_right_queue(uint8_t *buffer, int length)
{
	int pos = length - 1;

	uint8_t type = calculate_right_type(buffer[0]);

	switch(type)
	{
	case 0x01:
		printf("[IST] Answer=0x%02x\r", buffer[pos]);

		if(length > 1)
			enqueue(buffer[pos],&data_queue);
		else
			enqueue(buffer[pos],&answer_queue);
		break;
	case 0x02: 
		printf("[IST] Event=0x%02x\r", buffer[pos]);
		enqueue(buffer[pos],&event_queue);
		break;
	}
}

void decode_avr_packet(uint16_t *ptmp, int length)
{
	uint8_t tmpbuf[TEMP_BUFFER_SIZE];
	int tmppos = 0;
	uint16_t data;
	int ret;

	while (length-- > 0)
	{
		data = *ptmp++;
			
		if (data & 0x0100)
		{
			if (data & 0x0080)		// Startbyte
			{
				tmpbuf[0]=(uint8_t)data;
				tmppos=1;
				enqueue_to_right_queue(tmpbuf, tmppos);
			}
			else
			{
				if (tmppos > 0)		// Prüfsumme
				{
					if (avr_sum(tmpbuf, tmppos, RECI_SUM) == (uint8_t)data)
					{
						ret = calculate_right_type(tmpbuf[0]);

						/* 
						if(ret & 0x01)
							SetEvent(pAvrInitCont->hEv_AvrAnswer);

						if(ret & 0x02)
							SetEvent(pAvrInitCont->hEv_AvrEvent); 
						*/

						tmppos=0;
					}
					else
					{
						printf("[IST] Warning: CHKSUM error!\r\n");
					}
				}
				else 
				{
					printf("[IST] Warning: CHKSUM not received!\r\n");
					tmppos = 0;
				}
			}
		}
		else					
		{
			printf("[IST] Data=0x%02x\r\n", data);
			
			if ((tmppos > 0) && (tmppos < (TEMP_BUFFER_SIZE - 1)))
			{
				tmpbuf[tmppos++]=(uint8_t)data;	
				enqueue_to_right_queue(tmpbuf, tmppos);
				//Enqueue((BYTE)data,&DataQueue);
			}
			else 
				tmppos=0;
		}
	}	
}

void *read_from_avr (void *args)
{
	uint8_t buf[4];
	uint16_t avr_received;
	int count;

	printf("start reading from avr\n");

	while(1)
	{
		count = read(avr, buf, 2);
	
		if(count == 2)
		{
			printf("received 0x%x%02x,\n", buf[1], buf[0]);
			avr_received = (0xFF00 & (buf[1] << 8)) | buf[0];
			decode_avr_packet(&avr_received, 1);
		}
		else
		{
			printf("error: received %d,\n", count);
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

uint8_t avr_initialize(void)
{
	// *** open the device nodes ***
	avr = open_port();

	if(avr < 0) {
		return 0;
	}
	
	init_queue(&event_queue);
	init_queue(&answer_queue);
	init_queue(&data_queue);

	pthread_create(&read_thread, NULL, read_from_avr, NULL);
	
	return 1;
}

uint8_t avr_wait_and_check_answer(uint8_t command)
{
	uint8_t avr_answer;
	int timeout = 0;

	while(timeout++ < 100)
	{
		if(count_queue(&answer_queue) > 0)
			break;
			
		msleep(10);
	}
	
	if(timeout == 100)
		return 0;

	if (!dequeue(&avr_answer, &answer_queue))
	{
		printf("[AVR] Error: No AVR answer in queue\r\n");
		return 0;
	}

	if ((0xC0 | (command & 0x07)) != (0xC0 | (avr_answer & 0x07)))
	{
		printf("[AVR] Error: Wrong AVR Answer 0x%02x\r\n", avr_answer);
		return 0;
	}

	return 1;
}

uint8_t avr_send(uint8_t *send_buffer, int length)
{
	uint16_t  *tx_buffer;
	int i;
	int tx_buffer_lenth = length*2+2;

	// calculating sending sum
	uint8_t sum = avr_sum(send_buffer, length, SEND_SUM);
	
	// allocating new byte buffer and stuffing 9 bit transfer data
	tx_buffer = (uint16_t*)malloc(tx_buffer_lenth);

	if(tx_buffer == NULL)
	{
		return 0;
	}
	
	for (i = 0; i < length; i++)
		tx_buffer[i] = send_buffer[i];
		
	tx_buffer[0] |= 0x0100;
	tx_buffer[length] = sum | 0x0100;

	write(avr, tx_buffer, tx_buffer_lenth);
	
	free(tx_buffer);
	return 1;
}
	

uint8_t avr_transmit(uint8_t *send_buffer, int send_length,
	uint8_t *receive_buffer, int *receive_buffer_length)
{
	uint8_t avr_data;
	uint8_t *pTmp;
	int receive_count;
	
	if ( (send_buffer == NULL)||(receive_buffer == NULL) )
	{
		return 0;
	}

	while (dequeue(&avr_data, &answer_queue));
	while (dequeue(&avr_data, &data_queue));

	// enter critical section for AVR tranfer
	pthread_mutex_lock(&avr_mutex);

	// send command to AVR
	if (!avr_send(send_buffer, send_length))
	{
		pthread_mutex_unlock(&avr_mutex);
		return 0;
	}
	
	// wait for answer event
	if (!avr_wait_and_check_answer(send_buffer[0]))
	{
		pthread_mutex_unlock(&avr_mutex);
		return 0;
	}

	// get adn return data
	receive_count = 0;
	pTmp = receive_buffer; 
	while (dequeue(&avr_data, &data_queue) && *receive_buffer_length > receive_count)
	{
		*pTmp++ = avr_data;
		receive_count++;
	}
	
	*receive_buffer_length = receive_count;

	pthread_mutex_unlock(&avr_mutex);
	
	return 1;
}
	
uint8_t avr_get_events(uint8_t *receive_buffer, int *receive_buffer_length)
{
	uint8_t avr_event;
	int receive_count;
	
	if (receive_buffer == NULL)
	{
		return 0;
	}

	// enter critical section for AVR tranfer
	pthread_mutex_lock(&avr_mutex);

	receive_count = 0;
	while (dequeue(&avr_event, &event_queue) && *receive_buffer_length > receive_count)
	{
		*receive_buffer++ = avr_data;
		receive_count++;
	}
	
	*receive_buffer_length = receive_count;

	pthread_mutex_unlock(&avr_mutex);
	
	return 1;
}

uint8_t avr_reset_on(void)
{
	uint8_t read_buffer[2];
	uint8_t send_buffer[2] = { AVR_RESET_ON, 0 };
	
	return avr_transmit(send_buffer, 1, read_buffer, 4);
}

uint8_t avr_reset_off(void)
{
	uint8_t read_buffer[2];
	uint8_t send_buffer[2] = { AVR_RESET_OFF, 0 };
	
	return avr_transmit(send_buffer, 1, read_buffer, 4);
}

uint8_t avr_reset_all(void)
{
	uint8_t read_buffer[2];
	uint8_t send_buffer[2] = { AVR_RESET_ALL, 0 };
	
	return avr_transmit(send_buffer, 1, read_buffer, 4);
}
