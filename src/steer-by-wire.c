#include <unistd.h>	/*per sistem calls e pipe*/
#include <stdlib.h>	/*per funzione exit*/
#include <error.h>	/*per funzione perror*/
#include <sys/types.h>	/*per mkfifo*/
#include <sys/stat.h>	/*per mknod (sys call per mkfifo)*/
#include <string.h> /*per strcpy*/
#include <fcntl.h> /*per open*/

#define LEFT 0
#define RIGHT 1
#define INPUT_MAX_LEN 9
#define TURN_SECONDS 4

// La definizione delle successive macro puo` essere utile nei casi in cui si desidera modificare la frase da inserire nel file di log
/*lunghezza massima possibile della stringa da inserire nel log file compresa del carattere di ritorno a capo: "STO GIRANDO A SINISTRA\n" (DESTRA risulta piu` corta di un carattere)*/
#define LOG_MAX_LEN 23
/*lunghezza della parte comune di stringa da inserire nel log file*/
#define LOG_COMM_LEN 14

short int log_fd;

void turn ( char * , int);
void no_action ( );

/*La funzione main opera le operazioni di relative al componente steer-by-wire. Non richiede argomenti, quindi si omettono.*/
int main(){

	/*si crea il pipe con nome per la comunicazione con la central ECU previa eliminazione di eventuali hard links rimasti pendenti da eventuali esecuzioni precedenti*/
	unlink ("steer.pipe");
	mkfifo("steer.pipe", 0600);

	/*inizializzazione del file descriptor del pipe per la comunicazione tra central ECU e steer-by-wire. Il protocollo impone che il pipe sia creato durante la fase di inizializzazione del processo steer-by-wire e che venga aperto in sola scrittuta dal processo central-ECU il quale vi scriva al bisogno.*/
	int pipe_fd =  open ("../tmp/steer.pipe", O_RDONLY | O_NONBLOCK);

	log_fd = open("../log/steer.log", O_WRONLY | O_APPEND | O_CREAT, 0644);

	/*Definizione di un unico buffer, riempito inizialmente della parte di stringa comune la cui terminazione sara` cambiata di volta in volta*/
	char log_phrase[LOG_MAX_LEN];
	strcpy(log_phrase, "STO GIRANDO A ");

	// Inizializzazione della stringa di input, action, che rappresenta il comando impartito dalla central-ECU tramite una stringa di caratteri. La stringa massima ottenibile dal pipe e` "SINISTRA\0", lunga nove caratteri
	char action[INPUT_MAX_LEN];
	int nread;

	// Il ciclo infinito successivo rappresenta il cuore del processo. Ad ogni lettura del del pipe potra` accadere che il pipe in scrittura non sia ancora stato aperto (nread = -1), che il pipe sia aperto ma che non vi sia stato scritto niente (nread = 0), che nel pipe sia stato inserita la stringa "DESTRA" (nread = 7) o, infine, che sia stata inserita la stringa "SINISTRA" (nread = 8). L'assenza di stringhe nel pipe sara' interpretata come nessuna azione da eseguire dal componente, mentre sinistra e destra daranno l'avvio alla seguenza di svolta nella rispettiva direzione.
	// Il tempo trascorso tra una read e la successiva e` di un secondo nel caso di no_action e di 4 secondi nel caso di turn(). Gli input ricevuti nel mezzo di questi intervalli saranno ignorati.
	while(1){
		nread = read(pipe_fd, action, sizeof(action));
		switch(nread){
			case 8: turn(log_phrase, LEFT);
				break;
			case 7: turn(log_phrase, RIGHT);
				break;
			default: no_action();
		}
	}
}

void turn ( char * log_phrase, int direction){
	/*viene aggiunta alla stringa precedentemente definita log_phrase la corretta direzione da inserire nel log*/
	if(direction == LEFT)
		strcpy(&log_phrase[LOG_COMM_LEN], "SINISTRA\n");
	else
		strcpy(&log_phrase[LOG_COMM_LEN], "DESTRA\n");

	/*si esegue la scrittura della log_phrase in steer.log (fd = log_fd) per 4 volte (prossimi 4 secondi)*/
	for(int i = 0; i < TURN_SECONDS; i++){
		write(log_fd, log_phrase, LOG_MAX_LEN);
		sleep(1);
	}
	return;
}

/*si esegue la scrittura della frase "NO ACTION" in steer.log (fd = log_fd) per 1 volta (prossimo secondo)*/
void no_action( ){
	write (log_fd, "NO ACTION\n", 10);
	sleep(1);
	return;
}
