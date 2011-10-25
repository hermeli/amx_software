/***********************************************************************
						(C) KABA Access Management
 
 Driver:	avr.h

 Function:	AVR Driver - Interface for communicating (COM) with the AVR 
  			hardware.

 Autor:    	Lukas Landis (llandis@kbr.kaba.com)
 Date:   	03.10.2011

***********************************************************************/

#ifndef _LIBAVR_H_
#define _LIBAVR_H_

#include <stdint.h>

//-----------------------------------------------------------------------------
// AVR Communication Defines & Types
//-----------------------------------------------------------------------------
#define SEND_SUM			0x96
#define RECI_SUM			0xB2

#define AVR_CLEAR_EVENTS	0x81
#define AVR_WRITE			0x82
#define AVR_READ			0x83
#define AVR_RESET_ALL		0x85
#define AVR_RESET_ON		0x86
#define AVR_RESET_OFF		0x87
#define AVR_SETPIN			0x8A
#define	AVR_READHIGH		0x8B
#define	AVR_READLOW			0x8C
#define AVR_MODIFY			0x94

//#define DEBUG

#ifdef __cplusplus
extern "C" {
#endif

int avr_open(void);

uint8_t avr_reset_on(int ld);

uint8_t avr_reset_off(int ld);

uint8_t avr_reset_all(int ld);
	
uint8_t avr_transmit(int ld, uint8_t *send_buffer, int send_length,
	uint8_t *receive_buffer, int *receive_buffer_length);

uint8_t avr_clear_events(int ld);

uint8_t avr_wait_for_event(int ld);
	
uint8_t avr_get_events(int ld, uint8_t *receive_buffer, 
	int *receive_buffer_length);

uint8_t avr_read_addr(int ld, uint8_t addr, uint8_t *value);

uint8_t avr_close(int ld);

#ifdef __cplusplus
}
#endif

#endif
