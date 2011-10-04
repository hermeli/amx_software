
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include "../libavr/libavr.h"

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
