#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "../../include/service-functions.h"

// MACROS
// INPUT_LEN: numero fisso di byte da leggere dall'input
#define INPUT_LEN 8

// Funzione main che opera le operazioni per le componenti forward-facing-radar
// e per surround-view-cameras.
// Gli argomenti richiesti sono il nome della della modalita di esecuzione
// (scelto tra 0 e 1), il nome della componente (tra 0 e 1)  e il
// file descriptor del pipe o della socket (canale di comunicazione) su cui
// eseguire la trasmissione dei messaggi.
// Metodo di utilizzo:
// ./radar-cameras-sensor NORMALE/ARTIFICIALE RADAR/CAMERAS/ASSIST comm_fd
int main(int argc, char * argv[]){

	// Verifica della corretta chiamata del processo
	if(argc != 4){
		perror("chiamata");
		exit(EXIT_FAILURE);
	}

	// Inizializzazione del file descriptor da cui ottenere i bytes
	// di input dipendentemente dalla modalità selezionata.
	short int input_fd;
	if(argv[1][0] == NORMALE)
		input_fd = open("/dev/urandom", O_RDONLY);
	else if (argv[1][0] == ARTIFICIALE)
		input_fd = open("../../urandomARTIFICIALE.binary", O_RDONLY);
	else{
		perror("chiamata: modalita:");
		exit(EXIT_FAILURE);
	}

	// Verifica del corretto inserimento del file descriptor
	// del canale di comunicazione
	if(argv[3][0] < 0 ){
		perror("chiamata: canale comunicazione:");
		exit(EXIT_FAILURE);
	}
	short int comm_fd = argv[3][0];

  // Connessione del file descriptor del log file. Apertura in sola scrittura.
  // Qualora il file non esista viene creato. Qualora il file sia presente
  // si mantengono le precedenti scritture.
	short int log_fd;
	if(argv[2][0] == RADAR)
		log_fd = open("../log/radar.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
	else if (argv[2][0] == CAMERAS)
		log_fd = open("../log/cameras.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
	else if (argv[2][0] == ASSIST)
		log_fd = open("../log/assist.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
	else{
		perror("chiamata: tipologia funzione:");
		exit(EXIT_FAILURE);
	}

	// Inizializzazione della stringa di input che rappresenta l'insieme di byte
  // da trasmettere alla central-ECU da parte del processo.
  // Il +1 e` necessario per inserirvi il carattere di terminazione riga '\n'.
	char input_bytes[INPUT_LEN + 1];
  input_bytes[INPUT_LEN] = '\n';

	while (1) {
		if( (read(input_fd, input_bytes, INPUT_LEN)) == INPUT_LEN )
			broad_log(comm_fd, log_fd, input_bytes, INPUT_LEN + 1);
		sleep(1);
  }
}
