
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
//#include "../libavr/libavr.h"

extern int avr_open(void);
	
extern uint8_t avr_transmit(int ld, uint8_t *send_buffer, int send_length,
	uint8_t *receive_buffer, int *receive_buffer_length);
	
extern uint8_t avr_get_events(int ld, uint8_t *receive_buffer, 
	int *receive_buffer_length);

extern uint8_t avr_reset_on(int ld);

extern uint8_t avr_reset_off(int ld);

extern uint8_t avr_reset_all(int ld);

extern uint8_t avr_close(int ld);

int main(void)
{
	int avr_library_descriptor = avr_open();

	avr_reset_off(avr_library_descriptor);

	sleep(2);

	avr_reset_on(avr_library_descriptor);

	sleep(2);

	avr_close(avr_library_descriptor);

	return 0;
}
