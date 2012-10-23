
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include "../libavr/libavr.h"

int main(void)
{
	uint8_t buffer[20];
	int count;
	uint8_t exit_app = 0;
	int i;
	
	printf("open avr...\n");
	int avr_library_descriptor = avr_open();

	if(avr_library_descriptor == 0)
		return 1;
	
	printf("send reset on to avr...\n");
	avr_reset_on(avr_library_descriptor);

	sleep(1);
	
	printf("send reset off to avr...\n");
	avr_reset_off(avr_library_descriptor);

	sleep(1);

	printf("send reset all to avr...\n");
	avr_reset_all(avr_library_descriptor);

	sleep(1);
	
	avr_clear_events(avr_library_descriptor);
	
	while(1)
	{
		printf("wait for events...\n");
		avr_wait_for_event(avr_library_descriptor);

		count = sizeof(buffer);
		avr_get_events(avr_library_descriptor, buffer, &count);
		
		printf("event(s) received: ");
		
		for (i = 0; i < count; i++)
		{
			printf("%02x, ", buffer[i]);
			
			if(buffer[i] == 0xa3)
				exit_app = 1;
		}
		
		printf("\n");
		
		if(exit_app == 1)
			break;
	}
	
	avr_read_addr(avr_library_descriptor, 0x0c, buffer);
	
	printf("input IN1 state: %02x\n", buffer[0]);

	return 0;
	
	avr_close(avr_library_descriptor);
}
