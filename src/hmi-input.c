#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "../include/service-functions.h"

#define OUTPUT_MAX_LEN 12

unsigned short int acceptable_input ( char * );
void throttle_failed_handler( int );
void input_error_handler ( int );
void interrupt_handler( int );

bool interrupted_flag = false;
char failed_input_phrase[118] = "Digitazione del comando errata, inserire una "
						 "delle seguenti parole e premere invio:\n"
						 "- INIZIO\n"
						 "- PARCHEGGIO\n"
						 "- ARRESTO\n";
// La funzione main esegue le operazioni relative al componente
// di input, noto come human-machine-interface_input, abbreviato hmi-input
int main() {

	struct sigaction act = { 0 };
	act.sa_flags = SA_RESTART;
	act.sa_handler = &input_error_handler;
	sigaction(SIGUSR2, &act, NULL);
	signal(SIGUSR1, throttle_failed_handler);
	signal(SIGINT, interrupt_handler);

	// Connessione del file descriptor del pipe per la comunicazione tra central
  	// ECU e hmi-input. Il protocollo impone che il pipe sia creato
  	// durante la fase di inizializzazione del processo central-ECU e che venga
  	// aperto in sola scrittura dal processo hmi-input il quale vi
  	// scriva non appena riceva da tastiera attraverso il terminale di input.
	int pipe_fd;
	while((pipe_fd = openat(AT_FDCWD, "tmp/hmi-in.pipe", O_WRONLY | O_CREAT | O_NONBLOCK)) < 0){
		perror("hmi-input: openat pipe");
		sleep(1);
	}
	errno = 0;
	perror("hmi-input: per sport");
	perror("hmi-input: CONNECTED");
	// Inizializzazione della stringa di input che rappresenta il comando
	// inserito nel terminale da parte dell'esecutore del programma.
	// Il contenuto viene trasmesso alla central-ECU.
	char * term_input;
	if((term_input = malloc(OUTPUT_MAX_LEN)) == NULL){
		perror("hmi-input: malloc");
	}

	printf("TERMINALE DI INPUT\n\n"
				 "Inserire una delle seguenti parole e premere invio:\n"
				 "- INIZIO\n"
				 "- PARCHEGGIO\n"
				 "- ARRESTO\n");

	// Il ciclo successivo rappresenta il cuore del processo.
	// Il processo si mette in attesa di una stringa da parte
	// dello stdin (scanf) e una volta ottenuta la controlla per
	//rilevare se corrisponde ad un comando accettabile per il sistema e,
	// in caso di compatibilita, lo trasmette alla central-ECU,
	// altrimenti si mette in attesa di un nuovo comando.
	// L'inserimento del comando e` stato reso case insensitive
	while(true){
		if(fgets(term_input, OUTPUT_MAX_LEN, stdin) == NULL){
			perror("hmi-input: fgets");
			continue;
		}

		unsigned short int input_flag = acceptable_input(term_input);
		if(input_flag < 4) {
			if(write(pipe_fd, &input_flag, sizeof(unsigned short int)) < 0)
				perror("hmi-input: write");
		} else
			write(STDOUT_FILENO, failed_input_phrase, 118);
	}
}

// Funzione adibita al controllo del comando di input.
// Le stringhe sono rese interamente maiuscole e successivamente
// confrontate con i messaggi accettabili.
// I comandi accettati sono AVVIO, PARCHEGGIO e ARRESTO.
// Non e` stata utilizzata la funzione strcasecmp perché il programma
// favorisse una maggiore portabilità.
unsigned short int acceptable_input (char * input){
	char * upper = str_toupper(input);
	if(strcmp(upper, "INIZIO\n") == 0)
		return INIZIO;
	else if (strcmp(upper, "ARRESTO\n") == 0)
		return ARRESTO;
	else if (strcmp(upper, "PARCHEGGIO\n") == 0)
		return PARCHEGGIO;
	else
		return 4;
}

void throttle_failed_handler (int sig){
	write(STDOUT_FILENO, "\n", 1);
	exit(EXIT_SUCCESS);
}

void input_error_handler (int sig ){
		write(STDOUT_FILENO, failed_input_phrase, 118);
}

void interrupt_handler(int sig){
	if(kill(getppid(), SIGINT) < 0)
		perror("hmi-input: kill ppid");
	exit(EXIT_SUCCESS);
}

