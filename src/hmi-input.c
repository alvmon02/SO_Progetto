#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include "../include/service-functions.h"

// OUTPUT_MAX_LEN: lunghezza massima del messaggio ricevuto in input da tastiera
#define OUTPUT_MAX_LEN 11

bool acceptable_input ( char * );

// La funzione main esegue le operazioni relative al componente
// di input, noto come human-machine-interface_input, abbreviato hmi-input
int main() {

	// Connessione del file descriptor del pipe per la comunicazione tra central
  // ECU e hmi-input. Il protocollo impone che il pipe sia creato
  // durante la fase di inizializzazione del processo central-ECU e che venga
  // aperto in sola scrittura dal processo hmi-input il quale vi
  // scriva non appena riceva da tastiera attraverso il terminale di input.
	int pipe_fd = open("../tmp/hmi-in.pipe", O_WRONLY);

	// Inizializzazione della stringa di input che rappresenta il comando
	// inserito nel terminale da parte dell'esecutore del programma.
	// Il contenuto viene trasmesso alla central-ECU.
	char * term_input = malloc(OUTPUT_MAX_LEN);

	printf("TERMINALE DI INPUT\n\n");
	printf("Inserire una delle seguenti parole e premere invio:\n"
				 "INIZIO\n"
				 "PARCHEGGIO\n"
				 "ARRESTO\n");

	// Il ciclo successivo rappresenta il cuore del processo.
	// Il processo si mette in attesa di una stringa da parte
	// dello stdin (scanf) e una volta ottenuta la controlla per
	//rilevare se corrisponde ad un comando accettabile per il sistema e,
	// in caso di compatibilita, lo trasmette alla central-ECU,
	// altrimenti si mette in attesa di un nuovo comando.
	// L'inserimento del comando e` stato reso case insensitive
	while(1){
		scanf("%s", term_input);
		getchar();
		if(acceptable_input(term_input))
			write(pipe_fd, term_input, OUTPUT_MAX_LEN);
		else {
			printf("Digitazione del comando errata, inserire una "
						 "delle seguenti parole e premere invio:\n"
						 "INIZIO\n"
						 "PARCHEGGIO\n"
						 "ARRESTO\n");
		}
	}
}

// Funzione adibita al controllo del comando di input.
// Le stringhe sono rese interamente maiuscole e successivamente
// confrontate con i messaggi accettabili.
// I comandi accettati sono AVVIO, PARCHEGGIO e ARRESTO.
// Non e` stata utilizzata la funzione strcasecmp perché il programma
// favorisse una maggiore portabilità.
bool acceptable_input (char * input){
	char * upper = str_toupper(input);
	return strcmp(upper, "PARCHEGGIO"	) == 0
			|| strcmp(upper, "INIZIO"			) == 0
			|| strcmp(upper, "ARRESTO"		) == 0;
}
