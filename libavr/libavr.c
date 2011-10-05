/***********************************************************************
						(C) KABA Access Management

 Driver:	avr.c

 Function:	AVR Driver - Interface for communicating (COM) with the AVR 
  			hardware.

 Autor:    	Lukas Landis (llandis@kbr.kaba.com)
 Date:   	03.10.2011

***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <mqueue.h>
#include <string.h>

#include "libavr.h"

#define DEVICE 					"/dev/ttyS4"
#define QUEUE_MASK				511
#define SEND_BUFFER_SIZE		512
#define TEMP_BUFFER_SIZE		16
#define ANSWER_RECEIVED			"/areci"
#define EVENT_RECEIVED			"/ereci"
#define MQ_MSG_SIZE				10

typedef struct 
{
	uint8_t data[QUEUE_MASK + 1];
	int read;
	int write;
	pthread_mutex_t *mutex;
} queue_t;

typedef struct
{
	int port;

	queue_t event_queue;
	queue_t answer_queue;
	queue_t data_queue;

	pthread_t read_thread;
	int package_position;

	pthread_mutex_t queue_mutex;
	pthread_mutex_t avr_mutex;
} avr_struct_t, *avr_t;

uint8_t enqueue(uint8_t data, queue_t *queue)
{
	uint32_t next;
	
	pthread_mutex_lock(queue->mutex);
	
	next = ((queue->write + 1) & QUEUE_MASK);
	
	if (queue->read == next)
	{
		pthread_mutex_unlock(queue->mutex);
		return 0;
	}
	
	queue->data[queue->write] = data;
	queue->write = next;
	
	pthread_mutex_unlock(queue->mutex);
	return 1;
}

uint8_t dequeue(uint8_t *data, queue_t *queue)
{
	pthread_mutex_lock(queue->mutex);
	
	if (queue->read == queue->write)
	{
		pthread_mutex_unlock(queue->mutex);
		return 0;
	}
	
	*data = queue->data[queue->read];
	queue->read = ((queue->read + 1) & QUEUE_MASK);
	
	pthread_mutex_unlock(queue->mutex);
	return 1;
}

int count_queue(queue_t *queue)
{
	int count;
	
	pthread_mutex_lock(queue->mutex);
	
	count = ((queue->write - queue->read) & QUEUE_MASK);
	
	pthread_mutex_unlock(queue->mutex);
	
	return count;
}

void init_queue(pthread_mutex_t *mutex, queue_t *queue)
{
	queue->mutex = mutex;
	
	pthread_mutex_lock(queue->mutex);
	
	queue->write = 0;
	queue->read = 0;
	
	pthread_mutex_unlock(queue->mutex);
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

mqd_t open_event(const char *event)
{
	struct mq_attr attr;
	mqd_t mqd;
	
	attr.mq_maxmsg = 20;
	attr.mq_msgsize = MQ_MSG_SIZE;
	attr.mq_flags = 0;

	mqd = mq_open(event, O_RDWR | O_CREAT, 0664, &attr);

	if (mqd == -1){
		printf("mq_open failed! errno: %s\n", strerror(errno));
		return -1;
	}
	
	return mq_unlink(event);
}

int close_event(const char *event)
{
	mqd_t mqd = open_event(event);

	if (mqd == -1){
		return -1;
	}
	
	return mq_close(mqd);
}

int set_event(const char *event)
{
	mqd_t mqd;

	printf( "Start set_event.\n" );

	mqd = open_event(event);
	
	if (mqd == -1){
		printf("error: event open failed.\n");
		return 1;
	}

	printf("post event...\n");

	if (mq_send(mqd, "set", 3, 0) < 0)
		return 1;
	
	/*	
	printf("receive ok...\n");

	if(msgrcv(sem, &buff, 1, 200, 0) < 0)
		return 1;

	printf("delete event.\n");

	msgctl(sem, 0, IPC_RMID );
	*/
	
	printf("Stop.\n");

	return 0;
}

int sem_shared_wait_timed(mqd_t mqd, unsigned long timelimit)
{
	fd_set rfds;
	char buff[MQ_MSG_SIZE];
	struct timeval          timeOut;
	int                     nRet=0;
	
	printf("timelimit %lx\n", timelimit);

	timeOut.tv_sec  = timelimit / 1000;
	timeOut.tv_usec = (timelimit % 1000) * 1000;

	printf("segment id %x\n", mqd);
	
	FD_ZERO(&rfds);
	FD_SET(mqd, &rfds);

	printf("select starting...\n");
	nRet = select(mqd + 1, &rfds, NULL, NULL, &timeOut);
	
	printf("select returns %d\n", nRet);

	if(nRet <= 0)
		return 3;

	if(mq_receive(mqd, buff, MQ_MSG_SIZE, NULL) < 0)
	{
		printf("error mq_receive has errno: %d\n", errno);
		return 1;
	}
		
	return 0;
}

int wait_for_event(const char *event)
{
	char buff[MQ_MSG_SIZE];
	mqd_t mqd;

	printf( "Start wait_for_event.\n" );

	mqd = open_event(event);
	
	if (mqd == -1){
		printf("error: event open failed.\n");
		return 1;
	}

	printf( "wait for event...\n" );

	if(mq_receive(mqd, buff, MQ_MSG_SIZE, NULL) < 0)
	{
		printf("error mq_receive has errno: %d\n", errno);
		return 1;
	}
		
	/*
	printf( "send ok...\n" );

	buff.mtype = 200;
	if (msgsnd(sem, &buff, 1, 0) < 0)
		return 1;
	*/
		
	printf( "Stop.\n" );

	return 0;
}

int wait_for_event_timed(const char *event, unsigned long timeout)
{
	mqd_t mqd;

	printf( "Start wait_for_event_timed.\n" );
        
	mqd = open_event(event);
	
	if (mqd == -1){
		printf("error: event open failed.\n");
		return 1;
	}

	printf( "wait for event timed...\n" );

	if (sem_shared_wait_timed(mqd, timeout) == 3) 
	{
		printf("error: timeout\n");
		return 1;
	}
		
	/*
	printf( "send ok...\n" );

	buff.mtype = 200;
	if( msgsnd( sem, "set, 1, 0 ) < 0 )
		return 1;
	*/
		
	printf("Stop.\n");

	return 0;
}

uint8_t avr_sum(uint8_t *data, int count, uint8_t k)
{
	uint8_t sum = (*data++) - k;
	int n;

	for(n = 1; n < count; n++)
	{		
		sum = (sum & 0x80) ? 
			((sum << 1) - ((*data++) - 1)): 
			((sum << 1) - (*data++));
	}
	
	if (sum & 0x80){
		sum =~ sum;
	}
	
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

void enqueue_to_right_queue(avr_t avr, uint8_t *buffer, int length)
{
	int pos = length - 1;

	uint8_t type = calculate_right_type(buffer[0]);

	switch(type)
	{
	case 0x01:
		printf("[IST] Answer=0x%02x\n", buffer[pos]);

		if(length > 1)
			enqueue(buffer[pos],&(avr->data_queue));
		else
			enqueue(buffer[pos],&(avr->answer_queue));
		break;
	case 0x02: 
		printf("[IST] Event=0x%02x\n", buffer[pos]);
		enqueue(buffer[pos],&(avr->event_queue));
		break;
	}
}

void decode_avr_packet(avr_t avr, uint16_t *ptmp, int length)
{
	static uint8_t tmpbuf[TEMP_BUFFER_SIZE];
	uint8_t sum;
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
				avr->package_position = 1;
				
				enqueue_to_right_queue(avr, tmpbuf, 
					avr->package_position);
			}
			else
			{
				if (avr->package_position > 0)		// Prüfsumme
				{
					sum = avr_sum(tmpbuf, avr->package_position, 
						RECI_SUM);
					if (sum == (uint8_t)(data & 0x00FF))
					{
						ret = calculate_right_type(tmpbuf[0]);

						if(ret & 0x01)
							set_event(ANSWER_RECEIVED);

						if(ret & 0x02)
							set_event(EVENT_RECEIVED); 

						avr->package_position = 0;
					}
					else
					{
						printf("[IST] Warning: CHKSUM error (received: %x, calculated: %x)!\n", (uint8_t)(data & 0x00FF), sum);
						avr->package_position = 0;
					}
				}
				else 
				{
					printf("[IST] Warning: CHKSUM not received!\n");
					avr->package_position = 0;
				}
			}
		}
		else					
		{
			printf("[IST] Data=0x%02x\n", data);
			
			if ((avr->package_position > 0) && 
				(avr->package_position < (TEMP_BUFFER_SIZE - 1)))
			{
				tmpbuf[avr->package_position++] = (uint8_t)data;	
				enqueue_to_right_queue(avr, tmpbuf, 
					avr->package_position);
			}
			else 
				avr->package_position = 0;
		}
	}	
}

void *read_from_avr (void *args)
{
	avr_t avr = (avr_t)args;
	uint8_t buf[4];
	uint16_t avr_received;
	int count;

	printf("start reading from avr\n");

	while(1)
	{
		count = read(avr->port, buf, 2);
	
		if(count == 2)
		{
			avr_received = (0xFF00 & (buf[1] << 8)) | buf[0];
			printf("received 0x%x\n", avr_received);
			decode_avr_packet(avr, &avr_received, 1);
		}
		else
		{
			printf("error: received %d,\n", count);
			
			if(count == -1)
			{
				printf("error: errno: %d!", errno);
				
				if(errno == EBADF)
				{
					printf("error: port is not open for reading!");
					break;
				}
			}
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

int avr_open(void)
{
	// *** open the device nodes ***
	avr_t ld;
	int rc;
	
	printf("malloc structure... \n");
	ld = (avr_t)malloc(sizeof(avr_struct_t));
	
	if(ld == NULL)
	{
		printf("malloc failed\n");
		return 0;
	}
	
	printf("open port... \n");
	ld->port = open_port();

	if(ld->port < 0) {
		printf("open_port failed\n");
		free(ld);
		return 0;
	}
	
	printf("init mutex... \n");
	if((rc = pthread_mutex_init(&(ld->queue_mutex), NULL)))
	{
		printf("pthread_mutex_init failed\n");
		free(ld);
		return 0;
	}
	
	if((rc = pthread_mutex_init(&(ld->avr_mutex), NULL)))
	{
		printf("pthread_mutex_init failed\n");
		free(ld);
		return 0;
	}
	
	printf("init queue... \n");
	init_queue(&(ld->queue_mutex), &(ld->event_queue));
	init_queue(&(ld->queue_mutex), &(ld->answer_queue));
	init_queue(&(ld->queue_mutex), &(ld->data_queue));

	printf("starting thread... \n");
	ld->package_position = 0;
	pthread_create(&(ld->read_thread), NULL, read_from_avr, (void *)ld);
	
	return (int)ld;
}

uint8_t avr_wait_and_check_answer(avr_t avr, uint8_t command)
{
	uint8_t avr_answer;
	
	if(wait_for_event_timed(ANSWER_RECEIVED, 2000))
	{
		printf("[AVR] Error: Timeout, no answer received\r\n");
		return 0;
	}

	if (!dequeue(&avr_answer, &(avr->answer_queue)))
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

uint8_t avr_send(avr_t avr, uint8_t *send_buffer, int length)
{
	uint8_t  *tx_buffer;
	int i;
	int tx_buffer_length = length*2+2;

	// calculating sending sum
	uint8_t sum = avr_sum(send_buffer, length, SEND_SUM);
	
	// allocating new byte buffer and stuffing 9 bit transfer data
	tx_buffer = (uint8_t*)malloc(tx_buffer_length);

	if(tx_buffer == NULL)
	{
		return 0;
	}
	
	for (i = 0; i < length; i++)
	{
		tx_buffer[i * 2] = send_buffer[i];
		tx_buffer[(i * 2) + 1] = 0;
	}
		
	tx_buffer[1] |= 0x01;
	tx_buffer[tx_buffer_length - 2] = sum;
	tx_buffer[tx_buffer_length - 1] = 0x01;
	
	printf("sending: ");
		
	for (i = 0; i < tx_buffer_length; i++)
	{
		printf("%02x, ", tx_buffer[i]);
	}
	
	printf("\n");

	i = write(avr->port, tx_buffer, tx_buffer_length);
	
	if(i < 0)
	{
		printf("error: write returns errno: %d\n", errno);
	}
	
	
	free(tx_buffer);
	return 1;
}
	

uint8_t avr_transmit(int ld, uint8_t *send_buffer, int send_length,
	uint8_t *receive_buffer, int *receive_buffer_length)
{
	avr_t avr = (avr_t)ld;
	uint8_t avr_data;
	uint8_t *pTmp;
	int receive_count;
	
	if ( (send_buffer == NULL)||(receive_buffer == NULL) )
	{
		return 0;
	}

	while (dequeue(&avr_data, &(avr->answer_queue)));
	while (dequeue(&avr_data, &(avr->data_queue)));

	// enter critical section for AVR tranfer
	pthread_mutex_lock(&(avr->avr_mutex));
	
	close_event(ANSWER_RECEIVED);

	// send command to AVR
	if (!avr_send(avr, send_buffer, send_length))
	{
		pthread_mutex_unlock(&(avr->avr_mutex));
		return 0;
	}
	
	// wait for answer event
	if (!avr_wait_and_check_answer(avr, send_buffer[0]))
	{
		pthread_mutex_unlock(&(avr->avr_mutex));
		return 0;
	}

	// get adn return data
	receive_count = 0;
	pTmp = receive_buffer; 
	while (dequeue(&avr_data, &(avr->data_queue)) && *receive_buffer_length > receive_count)
	{
		*pTmp++ = avr_data;
		receive_count++;
	}
	
	*receive_buffer_length = receive_count;

	pthread_mutex_unlock(&(avr->avr_mutex));
	
	return 1;
}
	
uint8_t avr_get_events(int ld, uint8_t *receive_buffer, int *receive_buffer_length)
{
	avr_t avr = (avr_t)ld;
	uint8_t avr_event;
	int receive_count;
	
	if (receive_buffer == NULL)
	{
		return 0;
	}

	// enter critical section for AVR tranfer
	pthread_mutex_lock(&(avr->avr_mutex));

	receive_count = 0;
	while (dequeue(&avr_event, &(avr->event_queue)) && *receive_buffer_length > receive_count)
	{
		*receive_buffer++ = avr_event;
		receive_count++;
	}
	
	*receive_buffer_length = receive_count;

	pthread_mutex_unlock(&(avr->avr_mutex));
	
	return 1;
}

uint8_t avr_reset_on(int ld)
{
	uint8_t read_buffer[2];
	uint8_t send_buffer[2] = { AVR_RESET_ON, 0 };
	int read_buffer_length = 4;
	
	return avr_transmit(ld, send_buffer, 1, read_buffer, 
		&read_buffer_length);
}

uint8_t avr_reset_off(int ld)
{
	uint8_t read_buffer[2];
	uint8_t send_buffer[2] = { AVR_RESET_OFF, 0 };
	int read_buffer_length = 4;
	
	return avr_transmit(ld, send_buffer, 1, read_buffer,
		&read_buffer_length);
}

uint8_t avr_reset_all(int ld)
{
	uint8_t read_buffer[2];
	uint8_t send_buffer[2] = { AVR_RESET_ALL, 0 };
	int read_buffer_length = 4;
	
	return avr_transmit(ld, send_buffer, 1, read_buffer,
		&read_buffer_length);
}

uint8_t avr_close(int ld)
{
	avr_t avr = (avr_t)ld;
	
	pthread_mutex_lock(&(avr->avr_mutex));
	
	close(avr->port);
	
	pthread_mutex_unlock(&(avr->avr_mutex));
	
	pthread_mutex_destroy(&(avr->avr_mutex));
	pthread_mutex_destroy(&(avr->queue_mutex));
	
	free(avr);
	
	return 1;
}

uint8_t avr_wait_for_event(int ld)
{
	return wait_for_event(EVENT_RECEIVED);
}
