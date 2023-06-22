#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

// INPUT_MAX_LEN: lunghezza massima del messaggio ricevuto in input dalla central-ECU
#define INPUT_MAX_LEN 13

void throttle_failed_handler ( int );

// La funzione main esegue le operazioni relative al componente
// di output, noto come human-machine-interface_output, abbreviato hmi-output
int main() {

	signal(SIGUSR1, throttle_failed_handler);

	// Connessione del file descriptor del pipe per la comunicazione tra central
  // ECU e hmi-output. Il protocollo impone che il pipe sia creato
  // durante la fase di inizializzazione del processo central-ECU e che venga
  // aperto in sola lettura dal processo hmi-output il quale vi
  // legga non appena riceva un messaggio.
	int pipe_fd;
	if((pipe_fd = open("../tmp/hmi-out.pipe", O_RDONLY)) < 0){
		perror("open pipe");
		exit(EXIT_FAILURE);
	}

	// Inizializzazione della stringa di input che rappresenta
	// il messaggio da stampare a video sul terminale.
	// Il contenuto viene trasmesso dalla central-ECU.
	char * ECU_input = malloc(INPUT_MAX_LEN);
	if(ECU_input == NULL){
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	int nread;
	printf("TERMINALE DI OUTPUT\n\n");

	// Il ciclo successivo rappresenta il cuore del processo.
	// Il processo attende in attesa sul pipe che pervenga un messaggio da parte
	// della central-ECU e appena lo riceve questo viene immediatamente stampato
	// sul terminale da parte del processo.
	while(true)
		if((nread = read(pipe_fd, ECU_input, INPUT_MAX_LEN)) > 0 )
			printf("%s", ECU_input);
}

// Funzione per la gestione del segnale di errore
// Esegue la scrittura sul terminale tramite una write (vedi manuale signal-safety(7)) per mostrare l'interruzione del programma e la terminazione dell'intero programma.
void interrupt_handler ( int sig ){
	write(STDOUT_FILENO, "\nVEICOLO ARRESTATO.\nFallimento accelerazione.\nTerminazione totale esecuzione programma.\0", 90);
	exit(EXIT_SUCCESS);
}
