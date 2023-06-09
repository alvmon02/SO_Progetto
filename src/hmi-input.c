#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define OUTPUT_MAX_LEN 11

int main() {

	int pipe_fd = open("../tmp/hmi-in.pipe", O_WRONLY);

	char * ECU_output = malloc(OUTPUT_MAX_LEN);
	int nread;

	printf("TERMINALE DI INPUT\n\n");
	printf("Inserire una delle seguenti parole e premere invio:\n"
				 "INIZIO\n"
				 "PARCHEGGIO\n"
				 "ARRESTO\n");

	while(1){
		scanf("%s", ECU_output);
		if(strcasecmp(ECU_output, "PARCHEGGIO\n")
		|| strcasecmp(ECU_output,"INIZIO\n")
		|| strcasecmp(ECU_output, "ARRESTO\n") )
			write(pipe_fd, &ECU_output, OUTPUT_MAX_LEN);
		else {
			printf("Digitazione del comando errata, inserire una "
						 "delle seguenti parole e premere invio:\n"
						 "INIZIO\n"
						 "PARCHEGGIO\n"
						 "ARRESTO\n");
		}
	}
}

