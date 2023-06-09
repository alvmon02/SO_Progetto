#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define INPUT_MAX_LEN 13

int main() {

	int pipe_fd = open("../tmp/hmi-out.pipe", O_RDONLY);

	char * ECU_input = malloc(INPUT_MAX_LEN);
	int nread;

	printf("TERMINALE DI OUTPUT\n\n");
	while(1)
		if((nread = read(pipe_fd, ECU_input, INPUT_MAX_LEN)) > 0 )
			printf("%s", ECU_input);
}
