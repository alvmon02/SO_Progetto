#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include "../../include/service-functions.h"

// MACROS
// INPUT_MAX_LEN: lunghezza della stringa di input dalla central-ECU: "FRENO 5\n"
#define INPUT_MAX_LEN 10
// LOG_PHRASE_LEN: lunghezza della stringa di log: "DD/MM/YYYY hh:mm:ss - FRENO 5\n"
#define LOG_PHRASE_LEN 31

// File descriptor del log file log/brake.log
int log_fd;

void emergency_arrest ( int );

//La funzione main esegue le operazioni relative al componente brake-by-wire.c
int main ( ) {

	/*	Viene definito il gestore dei segnali che permette di eseguire, in caso di ricezione del segnale SIGUSR1
	 *	l'arresto d'emergenza con la funzione emergency_arrest
	 */
	struct sigaction act = { 0 };
	act.sa_flags = SA_RESTART;
	act.sa_handler = &emergency_arrest;
	sigaction(SIGUSR1, &act, NULL);

	/* Connessione del file descriptor del pipe per la comunicazione tra central ECU e brake-by-wire.
	 * Il protocollo impone che il pipe sia creato durante la fase di inizializzazione del processo central-ECU e che
	 * venga aperto in sola lettura dal processo brake-by-wire il quale vi legga al bisogno. */
	int pipe_fd;
	if((pipe_fd = openat(AT_FDCWD, "tmp/brake.pipe", O_RDONLY)) < 0){
		perror("open brake pipe");
		exit(EXIT_FAILURE);
	}

	/* Connessione del file descriptor del log file. Apertura in sola scrittura.
	 * Qualora il file non esista viene creato e se esiste viene sovrascritto dal nuovo file.
	 */
	if((log_fd = openat(AT_FDCWD, "log/brake.log", O_WRONLY | O_TRUNC | O_CREAT, 0644)) < 0){
		perror("open brake log");
		exit(EXIT_FAILURE);
	}

	/* Inizializzazione della stringa di input che rappresenta il comando
	 * impartito dalla central-ECU tramite una stringa di caratteri.
	 */
	char decrement[INPUT_MAX_LEN];
	int nread;

  /* Il ciclo infinito successivo rappresenta il cuore del processo.
   * Ad ogni lettura del pipe potra` accadere:
   * che il pipe in scrittura non sia ancora stato aperto (nread = -1),
   * che il pipe sia aperto ma che non vi sia stato ancora scritto niente (nread = 0),
   * che nel pipe sia stata inserita la stringa "FRENO 5\n" (nread > 0) che rappresenta
   * la richiesta di decremento della velocita' da parte della central-ECU.
   * Quest'ultimo caso da' inizio alla procedura simulativa della frenata.
   * Il pipe e` bloccante quindi il processo attende un messaggio dalla central-ECU
   */
	while(true){
		nread = read (pipe_fd, decrement, INPUT_MAX_LEN);
		if(nread < 0){
			perror("brake: read");
		}
		if(nread > 0){
			time_log_func( log_fd, LOG_PHRASE_LEN, BRAKE);
		}
	}
}

/* La funzione emergency_arrest simula una sequenza di arresto immediato
 * in caso di pericolo ricevuto dalla central-ECU attraverso la scrittura
 * nel log file della stringa "ARRESTO AUTO\n".
 * Questa costituisce il gestore (o handler) del segnale di pericolo
 * lanciato dalla central-ECU. 
 */
void  emergency_arrest ( int sig ){
	if(write(log_fd, "ARRESTO AUTO\n", 13) < 0){
		perror("brake: emergency arrest: write");
	}
}
