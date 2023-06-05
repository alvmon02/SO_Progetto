#include <error.h>     /*per funzione perror*/
#include <fcntl.h>     /*per open*/
#include <signal.h>    /*per la funzione kill*/
#include <stdio.h>     /*per sprintf*/
#include <stdlib.h>    /*per funzione exit, random, ecc*/
#include <sys/stat.h>  /*per mknod (sys call per mkfifo)*/
#include <sys/types.h> /*per mkfifo*/
#include <time.h> /*per le funzioni e le strutture relative a time, vedi logging ( ) */
#include <unistd.h> /*per sistem calls e pipe*/

#define INPUT_MAX_LEN 12
#define LOG_PHRASE_LEN 32

short int log_fd;

void log_func();
void check_failure();

int main() {

  /*si crea il pipe con nome per la comunicazione con la central ECU previa
   * eliminazione di eventuali hard links rimasti pendenti da eventuali
   * esecuzioni precedenti*/
  unlink("../tmp/throttle.pipe");
  mkfifo("../tmp/throttle.pipe", 0600);

  /*inizializzazione del file descriptor del pipe per la comunicazione tra
   * central ECU e throttle-control. Il protocollo impone che il pipe sia creato
   * durante la fase di inizializzazione del processo throttle-control e che
   * venga aperto in sola scrittuta dal processo central-ECU il quale vi scriva
   * al bisogno.*/
  short int pipe_fd = open("../tmp/throttle.pipe", O_RDONLY);
  log_fd = open("../log/throttle.log", O_WRONLY | O_APPEND | O_CREAT, 0644);

  // Inizializzazione della stringa di input, action, che rappresenta il comando
  // impartito dalla central-ECU tramite una stringa di caratteri. L'unica
  // stringa ottenibile tramite il pipe e` la stringa "INCREMENTO 5", lunga
  // dodici caratteri.
  char increment[INPUT_MAX_LEN];
  int nread;

  // Il ciclo infinito successivo rappresenta il cuore del processo. Ad ogni
  // lettura del del pipe potra` accadere che il pipe in scrittura non sia
  // ancora stato aperto (nread = -1), che il pipe sia aperto ma che non vi sia
  // stato scritto niente (nread = 0), che nel pipe sia stata inserita la
  // stringa "INCREMENTO 5" (nread > 0) che rappresenta la richiesta di
  // incremento della velocita' da parte della central-ECU. Quest'ultimo caso
  // da' inizio alla procedure di logging. Il tempo trascorso tra una read e la
  // successiva e` di un secondo in ogni caso.
  while (1) {
    if ((nread = read(pipe_fd, &increment, INPUT_MAX_LEN)) > 0) {
      check_failure();
      log_func();
    }
    sleep(1);
  }
}

// La funzione di logging alloca una stringa come buffer per poter scrivere nel
// file di log la data attuale, comprensiva dell'orario, e la stringa "AUMENTO
// 5\n". Dato che l'orario varia sempre risultava sconveniente predefinire la
// stringa nella funzione main (come e` stato fatto, ad esempio, in
// steer-by-wire.)
void log_func() {
  /*allocazione del buffer*/
  char *log_phrase = malloc(LOG_PHRASE_LEN);

  /*dichiarazione di raw_time, un tempo aritmtico utile per la costruzione di
   * act_time, e inizializzazione di questo al tempo aritmetico relativo al
   * momento attuale (di esecuzione, non di compilazione)*/
  time_t raw_time = time(NULL);

  /*struttura di time.h che mantiene campi distinti per ogni campo della data e
   * dell'orario*/
  struct tm act_time = *localtime(&raw_time);

  /*riempiemnto del buffer secondo il formato definito*/
  sprintf(log_phrase, "%02d/%02d/%d %02d:%02d:%02d - AUMENTO 5\n",
          act_time.tm_mday, act_time.tm_mon, act_time.tm_year + 1900,
          act_time.tm_hour, act_time.tm_min, act_time.tm_sec);

  /*scrittura nel log file*/
  write(log_fd, log_phrase, LOG_PHRASE_LEN);
  free(log_phrase);
  return;
}

// La funzione check_failure() implementa l'evento probabilistico ottenendo un
// numero casuale compreso in [0,(2^31)-1] ed eseguendone il modulo rispetto a
// 100000. In questo modo il numero ottenuto risulta un numero compreso tra 0 e
// 99999. Richiedendo che il numero ottenuto sia uguale a zero si simula la
// probabilita' che l'evento accada con una frequenza di una volta su 100000,
// ovvero che l'evento abbia probabilita' 10^-5. Se risulta vera l'uguaglianza
// descritta viene segnalato il processo padre. E' necessario notare che la
// generazione di numeri compresi tra 0 e 99999 non possiede una proporzione
// perfettamente lineare tra tutti i possibili valori in quanto, essendo random
// un generatore di numeri compresi tra 0 e 2^31 - 1 e supponendo che questo
// produca tutti i valori dell'intervallo con identica probabilita', allora
// risulta che i primi 83648 valori (resto della divisione 100000/2^31) verranno
// prodotti con una probabilita` appena maggiore, ovvero un valore in [0, 83647]
// viene prodotto 21475/21474 volte in piu' di un valore compreso in
// [83648,99999].
void check_failure() {
  if ((rand() % 100000) == 0) {
    kill(getppid(), SIGCHLD);
  }
}
