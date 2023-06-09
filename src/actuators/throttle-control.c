#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

// MACROS
// INPUT_MAX_LEN: lunghezza della stringa di input dalla central-ECU: "INCREMENTO 5\n"
#define INPUT_MAX_LEN 13
// LOG_PHRASE_LEN: lunghezza della stringa di log: "DD/MM/YYYY hh:mm:ss - AUMENTO 5\n"
#define LOG_PHRASE_LEN 32

// File descriptor del log file
short int log_fd;

void log_func ( );
bool throttle_failed ( );

//La funzione main esegue le operazioni relative al componente throttle-control.c
int main() {

  /* Connessione del file descriptor del pipe per la comunicazione tra central ECU e throttle-control.
   * Il protocollo impone che il pipe sia creato durante la fase di inizializzazione del processo central-ECU e che
   * venga aperto in sola lettura dal processo throttle-control il quale vi legga al bisogno. */
  short int pipe_fd = open("../tmp/throttle.pipe", O_RDONLY);
  log_fd = open("../log/throttle.log", O_WRONLY | O_APPEND | O_CREAT, 0644);

  /* Inizializzazione della stringa di input che rappresenta il comando
   * impartito dalla central-ECU tramite una stringa di caratteri. */
  char increment[INPUT_MAX_LEN];
  int nread;

  /* Il ciclo infinito successivo rappresenta il cuore del processo.
   * Ad ogni lettura del del pipe potra` accadere:
   * che il pipe in scrittura non sia ancora stato aperto (nread = -1),
   * che il pipe sia aperto ma che non vi sia stato ancora scritto niente (nread = 0),
   * che nel pipe sia stata inserita la stringa "INCREMENTO 5\n" (nread > 0) che rappresenta
   * la richiesta di incremento della velocita' da parte della central-ECU.
   * Quest'ultimo caso da' inizio alla procedura simulativa dell'incremento.
   * Il pipe e` bloccante quindi il processo attende un messaggio dalla central-ECU */
  while (1) {
    if ((nread = read(pipe_fd, &increment, INPUT_MAX_LEN)) > 0) {
      if(!throttle_failed())
        log_func();
    }
  }
}

/* La funzione di logging alloca una stringa come buffer per poter scrivere nel file di log
 * la data attuale, comprensiva dell'orario, e la stringa "AUMENTO 5\n" */
void log_func() {
	// Allocazione del buffer per la formattazione della stringa da inserire nel log file
	char *log_phrase = malloc(LOG_PHRASE_LEN);

  /* Dichiarazione di raw_time, un tempo aritmtico utile per la costruzione di
   * act_time, e inizializzazione di questo al tempo aritmetico relativo al
   * momento attuale di esecuzione time(NULL)*/
  time_t raw_time = time(NULL);

  // Struttura di time.h che mantiene campi distinti per ogni campo della data e dell'orario
  struct tm act_time = *localtime(&raw_time);

  // Riempiemnto del buffer secondo il formato definito
  sprintf(log_phrase, "%02d/%02d/%d %02d:%02d:%02d - AUMENTO 5\n",
          act_time.tm_mday, act_time.tm_mon, act_time.tm_year + 1900,
          act_time.tm_hour, act_time.tm_min, act_time.tm_sec);

  // Scrittura nel log file
  write(log_fd, log_phrase, LOG_PHRASE_LEN);
  free(log_phrase);
  return;
}

/* La funzione throttle_failed() implementa l'evento probabilistico ottenendo un
 * numero casuale compreso in [0,(2^31)-1] ed eseguendone il modulo rispetto a
 * 100'000. In questo modo il numero ottenuto risulta un numero compreso tra 0 e
 * 99999. Richiedendo che il numero ottenuto sia uguale a zero si simula la
 * probabilita' che l'evento accada con una frequenza di una volta su 100'000,
 * ovvero che l'evento abbia probabilita' 10^-5. Se risulta vera l'uguaglianza
 * descritta viene segnalato al processo central-ECU tramite SIGUSR1.
 * E' necessario notare che la generazione di numeri compresi tra 0 e 99999 non possiede
 * una proporzione perfettamente lineare tra tutti i possibili valori in quanto,
 * essendo random un generatore di numeri compresi tra 0 e 2^31 - 1 e supponendo
 * che questo produca tutti i valori dell'intervallo con identica probabilita',
 * allora risulta che i primi 83648 valori (resto della divisione 100000/2^31) verranno
 * prodotti con una probabilita` appena maggiore, ovvero un valore in [0, 83647]
 * viene prodotto 21475/21474 volte in piu' di un valore compreso in [83648,99999].
 * Ai fini del programma risulta comunque una differenza minima che
 * non ne` compromette il funzionamento */
bool throttle_failed() {
  if ((rand() % 100000) == 0) {
    kill(getppid(), SIGUSR1);
    return true;
  }
  return false;
}
