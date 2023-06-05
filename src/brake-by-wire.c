#include <unistd.h>	/*per sistem calls e pipe*/
#include <stdlib.h>	/*per funzione exit*/
#include <error.h>	/*per funzione perror*/
#include <sys/types.h>	/*per mkfifo*/
#include <sys/stat.h>	/*per mknod (sys call per mkfifo)*/
#include <stdio.h> /*per sprintf*/
#include <fcntl.h> /*per open*/
#include <time.h> /*per le funzioni e le strutture relative a time, vedi logging ( ) */
#include <signal.h> /*per la funzione kill*/

#define INPUT_MAX_LEN 8
#define LOG_PHRASE_LEN 30

short int log_fd;

void log_func ( );
void emergency_arrest ( int );

int main(){	// COMMENTI DA CAMBIARE TUTTI!

	/*si crea il pipe con nome per la comunicazione con la central ECU previa eliminazione di eventuali hard links rimasti pendenti da eventuali esecuzioni precedenti*/
	unlink ("../tmp/brake.pipe");
	mkfifo("../tmp/brake.pipe", 0600);

	/*inizializzazione del file descriptor del pipe per la comunicazione tra central ECU e throttle-control. Il protocollo impone che il pipe sia creato durante la fase di inizializzazione del processo throttle-control e che venga aperto in sola scrittuta dal processo central-ECU il quale vi scriva al bisogno.*/
	short int pipe_fd =  open ("../tmp/brake.pipe", O_RDONLY);
	log_fd = open("../log/brake.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
	signal (SIGUSR1, &emergency_arrest);

	// Inizializzazione della stringa di input, action, che rappresenta il comando impartito dalla central-ECU tramite una stringa di caratteri. L'unica stringa ottenibile tramite il pipe e` la stringa "INCREMENTO 5", lunga undici caratteri.
	char increment[INPUT_MAX_LEN];
	int nread;

	// Il ciclo infinito successivo rappresenta il cuore del processo. Ad ogni lettura del del pipe potra` accadere che il pipe in scrittura non sia ancora stato aperto (nread = -1), che il pipe sia aperto ma che non vi sia stato scritto niente (nread = 0), che nel pipe sia stata inserita la stringa "INCREMENTO 5" (nread > 0) che rappresenta la richiesta di incremento della velocita' da parte della central-ECU. Quest'ultimo caso da' inizio alla procedure di logging.
	// Il tempo trascorso tra una read e la successiva e` di un secondo in ogni caso.
	while(1){
		if((nread = read (pipe_fd, &increment, INPUT_MAX_LEN))>0){
			log_func( );
		}
		sleep(1);
	}
}

// La funzione di logging alloca una stringa come buffer per poter scrivere nel file di log la data attuale, comprensiva dell'orario, e la stringa "AUMENTO 5\n". Dato che l'orario varia sempre risultava sconveniente predefinire la stringa nella funzione main (come e` stato fatto, ad esempio, in steer-by-wire.)
void log_func ( ){
	/*allocazione del buffer*/
	char *log_phrase = malloc(LOG_PHRASE_LEN);

	/*dichiarazione di raw_time, un tempo aritmtico utile per la costruzione di act_time, e inizializzazione di questo al tempo aritmetico relativo al momento attuale (di esecuzione, non di compilazione)*/
	time_t raw_time = time(NULL);

	/*struttura di time.h che mantiene campi distinti per ogni campo della data e dell'orario*/
	struct tm act_time = *localtime(&raw_time);

	/*riempiemnto del buffer secondo il formato definito*/
	sprintf(log_phrase, "%02d/%02d/%d %02d:%02d:%02d - FRENO 5\n", act_time.tm_mday, act_time.tm_mon, act_time.tm_year + 1900, act_time.tm_hour, act_time.tm_min, act_time.tm_sec);

	/*scrittura nel log file*/
	write(log_fd, log_phrase, LOG_PHRASE_LEN);
	free (log_phrase);
	return;
}

void  emergency_arrest ( int signum ){
	write(log_fd, "ARRESTO AUTO\n", 8);
	return;
}

