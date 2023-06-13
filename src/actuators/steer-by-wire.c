#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

// MACROS
#define LEFT 0
#define RIGHT 1
// INPUT_MAX_LEN: lunghezza massima della stringa di input dalla central-ECU: "SINISTRA\n"
#define INPUT_MAX_LEN 9
// TURN_SECONDS: numero di secondi nei quali l'attuatore dovra` eseguire la svolta non considerando gli input
#define TURN_SECONDS 4
// LOG_MAX_LEN: lunghezza massima della stringa da inserire nel log file, compresa del carattere '\n': "STO GIRANDO A SINISTRA\n" (DESTRA risulta piu` corta di un carattere)
#define LOG_MAX_LEN 23
// LOG_COMM_LEN: lunghezza della parte comune della stringa da inserire nel log file nei due casi di svolta a sinistra e di svolta a destra
#define LOG_COMM_LEN 14

// File descriptor del log file
short int log_fd;

void turn ( char * , int);
void no_action ( );

//La funzione main esegue le operazioni relative al componente steer-by-wire.
int main(){

	/* Connessione del file descriptor del pipe per la comunicazione tra central ECU e steer-by-wire.
	 * Il protocollo impone che il pipe sia creato durante la fase di inizializzazione del processo central-ECU e che
	 * venga aperto in sola lettura dal processo steer-by-wire il quale vi legga al bisogno. */
	int pipe_fd =  open ("../tmp/steer.pipe", O_RDONLY | O_NONBLOCK);

	// Creazione e apertura del log file e associazione del file descriptor
	log_fd = open("../log/steer.log", O_WRONLY | O_APPEND | O_CREAT, 0644);

	// Definizione di un buffer riempito inizialmente della parte di stringa comune la cui terminazione sara` modificata ad ogni svolta
	char log_phrase[LOG_MAX_LEN];
	strcpy(log_phrase, "STO GIRANDO A ");

	// Inizializzazione della stringa di input che rappresenta il comando impartito dalla central-ECU tramite una stringa di caratteri
	char action[INPUT_MAX_LEN];
	int nread;

	/* Il ciclo infinito successivo rappresenta il cuore del processo.
	 * Ad ogni lettura del del pipe potra` accadere che il pipe in scrittura non sia ancora stato aperto (nread = -1),
	 * che il pipe sia aperto ma che non vi sia stato scritto niente (nread = 0),
	 * che nel pipe sia stato inserita la stringa "DESTRA\n" (nread = 7) o, infine,
	 * che sia stata inserita la stringa "SINISTRA\n" (nread = 8).
	 * L'assenza di stringhe nel pipe sara' interpretata come nessuna azione da eseguire dall'attuatore
	 * mentre "SINISTRA\n" e "DESTRA\n" daranno l'avvio alla seguenza di svolta nella rispettiva direzione.
	 * Il tempo trascorso tra una read e la successiva e` di un secondo nel caso di no_action e di 4 secondi nel caso di turn().
	 * Gli input ricevuti nel mezzo di questi intervalli saranno ignorati. */
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

// La funzione simula la svolta scrivendo nel log file, per 4 secondi, la frase relativa alla direzione di svolta
void turn ( char * log_phrase, int direction){
	// Viene aggiunta alla stringa precedentemente definita log_phrase la corretta direzione da inserire nel log
	if(direction == LEFT)
		strcpy(&log_phrase[LOG_COMM_LEN], "SINISTRA\n");
	else
		strcpy(&log_phrase[LOG_COMM_LEN], "DESTRA\n");

	// Si esegue la scrittura della log_phrase in steer.log per 4 volte (una volta al secondo per 4 secondi)
	for(int i = 0; i < TURN_SECONDS; i++){
		write(log_fd, log_phrase, LOG_MAX_LEN);
		sleep(1);
	}
	return;
}

// La funzione esegue la scrittura della frase "NO ACTION" in steer.log (fd = log_fd) per 1 volta e attende un secondo
void no_action( ){
	write (log_fd, "NO ACTION\n", 10);
	sleep(1);
	return;
}