#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

// INPUT_MAX_LEN: lunghezza massima del messaggio ricevuto in input dalla central-ECU
#define INPUT_MAX_LEN 150

void throttle_failed_handler ( int );
void park_handler (int );
// int read_line(char *, int, int);

// La funzione main esegue le operazioni relative al componente
// di output, noto come human-machine-interface_output, abbreviato hmi-output
int main() {

	signal(SIGUSR1, throttle_failed_handler);
	signal(SIGINT, park_handler);

	// Connessione del file descriptor del pipe per la comunicazione tra central
  // ECU e hmi-output. Il protocollo impone che il pipe sia creato
  // durante la fase di inizializzazione del processo central-ECU e che venga
  // aperto in sola lettura dal processo hmi-output il quale vi
  // legga non appena riceva un messaggio.
	FILE * pipe;
	// int pipe_fd;
	if((pipe = fopen("tmp/hmi-out.pipe", "r")) < 0)
		perror("hmi-output: open pipe");
	// Inizializzazione della stringa di input che rappresenta
	// il messaggio da stampare a video sul terminale.
	// Il contenuto viene trasmesso dalla central-ECU.
	char * ECU_input = malloc(INPUT_MAX_LEN);
	if(ECU_input == NULL)
		perror("malloc");
	// int nread;
	printf("TERMINALE DI OUTPUT\n\n");

	// Il ciclo successivo rappresenta il cuore del processo.
	// Il processo attende in attesa sul pipe che pervenga un messaggio da parte
	// della central-ECU e appena lo riceve questo viene immediatamente stampato
	// sul terminale da parte del processo.

	while(true){
		if(read(pipe->_fileno, ECU_input, INPUT_MAX_LEN) >= 0){
			printf("%s", ECU_input);
			memset(ECU_input, 0, INPUT_MAX_LEN);
		} else{
			perror("hmi-out: read error");
			exit(EXIT_FAILURE);
		}
	}
}

// Funzione per la gestione del segnale di errore
// Esegue la scrittura sul terminale tramite una write (vedi manuale signal-safety(7)) per mostrare l'interruzione del programma e la terminazione dell'intero programma.
void throttle_failed_handler ( int sig ){
	write(STDOUT_FILENO, "\nVEICOLO ARRESTATO.\nFallimento accelerazione.\nTerminazione totale del programma.\n", 81);
	exit(EXIT_SUCCESS);
}

void park_handler( int sig ) {
	write(STDOUT_FILENO, "\nPARCHEGGIO COMPLETATO.\nTerminazione totale del programma.\n", 59);
	exit(EXIT_SUCCESS);
}


// int read_line(char *string, int string_length, int pipe_fd) {
// 	int i = 0;
// 	if(read(pipe_fd, string, string_length) > 0 ) {
// 		for(i = 0; string[i-1] != '\0' || i < string_length; i++) {
// 			if(string[i-1] == '\n')
// 				string[i] = '\0';
// 		}
// 		return 0;
// 	}
// 	return -1;
// }
