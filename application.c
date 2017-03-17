#include <stdio.h>
#include <unistd.h>

int main()
{
	while (1) {
		fprintf(stderr, ".");
		sleep(256); // we will try to nop this
	}

	return 0;
}
