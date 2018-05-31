#include <avr/io.h>
#include <stdlib.h>

#include "system.h"

int main (void)
{
	blinkencat.initialize();

	while (1) {
		blinkencat.loop();
	}

	return 0;
}
