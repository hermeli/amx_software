
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include "../libavr/libavr.h"

int main(void)
{
	printf("open avr...\n");
	int avr_library_descriptor = avr_open();

	if(avr_library_descriptor == 0)
		return 1;

	printf("send reset all to avr...\n");
	avr_reset_all(avr_library_descriptor);

	sleep(2);
	
	printf("send reset off to avr...\n");
	avr_reset_off(avr_library_descriptor);

	sleep(2);
	
	printf("wait for events...\n");
	avr_wait_for_event(avr_library_descriptor);

	avr_close(avr_library_descriptor);

	return 0;
}
