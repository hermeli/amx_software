/***********************************************************************
						(C) KABA Access Management
 
 Driver:	avr.h

 Function:	AVR Driver - Interface for communicating (COM) with the AVR 
  			hardware.

 Autor:    	Lukas Landis (llandis@kbr.kaba.com)
 Date:   	03.10.2011

***********************************************************************/

#ifndef _AVR_H_
#define _AVR_H_

//-----------------------------------------------------------------------------
// AVR Communication Defines & Types
//-----------------------------------------------------------------------------
#define SEND_SUM			0x96
#define RECI_SUM			0xB2

#define AVR_WRITE			0x82
#define AVR_READ			0x83
#define AVR_MODIFY			0x94
#define AVR_RESET_ALL		0x85
#define AVR_RESET_ON		0x86
#define AVR_RESET_OFF		0x87
#define AVR_SETPIN			0x8A
#define	AVR_READHIGH		0x8B
#define	AVR_READLOW			0x8C

#ifdef __cplusplus
extern "C" {
#endif

uint8_t avr_initialize(void);
	
uint8_t avr_transmit(uint8_t *send_buffer, int send_length,
	uint8_t *receive_buffer, int *receive_buffer_length);
	
uint8_t avr_get_events(uint8_t *receive_buffer, int *receive_buffer_length);

uint8_t avr_reset_on(void);

uint8_t avr_reset_off(void);

uint8_t avr_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif
