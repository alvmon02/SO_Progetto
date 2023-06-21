#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include "../../include/service-functions.h"
// MACROS
// INPUT_MAX_LEN: lunghezza della stringa di input dalla central-ECU:
// "INCREMENTO 5\n"
#define INPUT_MAX_LEN 13
// LOG_PHRASE_LEN: lunghezza della stringa di log:
// "DD/MM/YYYY hh:mm:ss - AUMENTO 5\n"
#define LOG_PHRASE_LEN 32

// File descriptor del log file
short int log_fd;

bool throttle_failed ( );

// La funzione main esegue le operazioni relative al
// componente throttle-control.c
int main() {

  /* Connessione del file descriptor del pipe per la comunicazione tra
   * central ECU e throttle-control.
   * Il protocollo impone che il pipe sia creato durante la fase di
   * inizializzazione del processo central-ECU e che
   * venga aperto in sola lettura dal processo throttle-control il quale vi
   * legga al bisogno. */
  int pipe_fd;
  if((pipe_fd = ("../../tmp/throttle.pipe", O_RDONLY))){
    perror("open pipe");
    exit(EXIT_FAILURE);
  }
  if((log_fd = open("../../log/throttle.log", O_WRONLY | O_APPEND | O_CREAT, 0644))){
    perror("open log");
    exit(EXIT_FAILURE);
  }

  /* Inizializzazione della stringa di input che rappresenta il comando
   * impartito dalla central-ECU tramite una stringa di caratteri. */
  char increment[INPUT_MAX_LEN];
  int nread;

  /* Il ciclo infinito successivo rappresenta il cuore del processo.
   * Ad ogni lettura del del pipe potrà accadere:
   * che il pipe in scrittura non sia ancora stato aperto (nread = -1),
   * che il pipe sia aperto ma che non vi sia stato ancora scritto niente (nread = 0),
   * che nel pipe sia stata inserita la stringa "INCREMENTO 5\n" (nread > 0) che rappresenta
   * la richiesta di incremento della velocità da parte della central-ECU.
   * Quest'ultimo caso da' inizio alla procedura simulativa dell'incremento.
   * Il pipe e` bloccante quindi il processo attende un messaggio dalla
central-ECU */
  while (1) {
    if ((nread = read(pipe_fd, &increment, INPUT_MAX_LEN)) > 0) {
      if(!throttle_failed())
        time_log_func(log_fd, LOG_PHRASE_LEN, THROTTLE);
    }
  }
}

/* La funzione throttle_failed() implementa l'evento probabilistico ottenendo un
 * numero casuale compreso in [0,(2^31)-1] ed eseguendone il modulo rispetto a
 * 100'000. In questo modo il numero ottenuto risulta un numero compreso tra 0 e
 * 99999. Richiedendo che il numero ottenuto sia uguale a zero si simula la
 * probabilita' che l'evento accada con una frequenza di una volta su 100'000,
 * ovvero che l'evento abbia probabilita' 10^-5. Se risulta vera l'uguaglianza
 * descritta viene segnalato al processo central-ECU tramite SIGUSR1.
 * E' necessario notare che la generazione di numeri compresi tra 0 e 99999 non
 * possiede una proporzione perfettamente lineare tra tutti i possibili valori
 * in quanto, essendo random un generatore di numeri compresi tra 0 e 2^31 - 1
 * e supponendo che questo produca tutti i valori dell'intervallo con identica
 * probabilita', allora risulta che i primi 83648 valori (resto della
 * divisione 100000/2^31) verranno prodotti con una probabilita` appena
 * maggiore, ovvero un valore in [0, 83647] viene prodotto 21475/21474 volte in
 * piu' di un valore compreso in [83648,99999].
 * Ai fini del programma risulta comunque una differenza minima che
 * non ne` compromette il funzionamento */
bool throttle_failed() {
  if ((rand() % 100000) == 0) {
    kill(getppid(), SIGUSR1);
    return true;
  }
  return false;
}
