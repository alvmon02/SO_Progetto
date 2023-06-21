/*Inclusione delle librerie necessarie*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include "../include/service-functions.h"

// Funzione per l'inizializzazione delle socket.
// La presente funzione esegue il processo di inizializzazione della socket dalla parte del
// server eseguendo le operazioni di unlink, socket, bind e listen.
// In caso avvenga un errore in una delle suddette funzioni viene scritto lo stato
// dell'errore nel errno e stampato sullo stderr il nome della funzione tramite perror
int initialize_socket(char * sock_pathname, int domain, int type, int queue_len){
	int fd;
	if(unlink(sock_pathname)){
		perror("unlink");
		exit(EXIT_FAILURE);
	}
	if((fd = socket(domain, type, 0))){
		perror("unlink");
		exit(EXIT_FAILURE);
	}
	struct sockaddr_un addr;
	addr.sun_family = domain;
	strcpy(addr.sun_path, sock_pathname);
	if (!bind(fd, (struct sockaddr *) &addr, sizeof(addr))) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	if (!listen (fd, queue_len)){
		perror("listen");
		exit(EXIT_FAILURE);
	}
	return fd;
}

// Funzione per l'inizializzazione delle pipe.
// La presente funzione esegue la parte di inizializzazione delle pipe.
// Questa viene eseguita da uno solo dei processi che si connettono alla pipe,
// infatti esegue la funzione unlink, la funzione mkfifo e la open con flags specificati come parametro.
int initialize_pipe(char * pipe_pathname, int flags, mode_t mode){
	if(unlink (pipe_pathname)){
		perror("unlink");
		exit(EXIT_FAILURE);
	}
	if(mkfifo(pipe_pathname, mode)){
		perror("mkfifo");
		exit(EXIT_FAILURE);
	}
	int fd;
	if((fd = open(pipe_pathname, flags))){
		perror("open");
		exit(EXIT_FAILURE);
	}
	return fd;
}

// Funzione per la lettura di messaggi tramite pipe
void read_output (int fd, char * message_out, size_t size){
	read (fd, message_out, size);
}

// Funzione che converte una stringa binaria di bytes (unsigned char *)
// in codifica esadecimale.
void hex ( unsigned char* to_conv, size_t size, char* converted){
	for (int i = 0; i < size; i++){
		converted[2*i] = to_conv[i]/16 + 48;
		converted[(2*i) + 1] = to_conv[i]%16 + 48;
		if(converted[2*i] > 57)
			converted[2*i] += 7;
		if(converted[(2*i) + 1] > 57)
			converted[(2*i) + 1] += 7;
	}
}

// Funzione per l'esecuzione sequenziale di invio tramite pipe di un messaggio
// e la scrittura dello stesso nel log file.
void broad_log (int pipe_fd, int log_fd, char * message, size_t size){
	if(write (pipe_fd, message, size) || write (log_fd,	message, size)){
		perror("write");
		exit(EXIT_FAILURE);
	}
}

// Funzione utilizzata da byte-sensors.c e park-assist.c.
// La funzione esegue la lettura dei bytes binari dal sorgente in input_fd e li invia, se sono esattamente
// 8 bytes, tramite il canale di comunicazione comm_fd e li scrive nel file di log rappresentato
// dal file descriptor log_fd. Sfrutta le funzioni hex(unsigned char *, size_t, char *) e la funzione
// broad_log(int, int, char *, size_t)
void read_conv_broad(int input_fd, unsigned char * input_str, char * input_hex, int comm_fd, int log_fd){
	if( read(input_fd, input_str, BYTES_LEN) == BYTES_LEN ){
			hex(input_str, BYTES_LEN, input_hex);
			broad_log(comm_fd, log_fd, input_hex, (BYTES_LEN *2) + 1);
		}
		sleep(1);
}

// str_toupper restituisce una stringa equivalente alla stringa str
// composta di soli caratteri maiuscoli. Sfrutta la funzione posix toupper(char).
char * str_toupper(char * str) {
	for(char * p = str; *p != 0; p++)
		*p = toupper(*p);
	return str;
}

void time_log_func (int log_fd, size_t size, short int proc ){
	// Allocazione del buffer per la formattazione della stringa da inserire
  // nel log file
	char *log_phrase = malloc(size);
  if(log_phrase == NULL){
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  /* Dichiarazione di raw_time, un tempo aritmetico utile per la costruzione di
   * act_time, e inizializzazione di questo al tempo aritmetico relativo al
   * momento attuale di esecuzione time(NULL)*/
	time_t raw_time = time(NULL);
	if(raw_time == (time_t) -1){
		perror("time");
		exit(EXIT_FAILURE);
	}

  // Struttura di time.h che mantiene campi distinti per ogni campo della data
  // e dell'orario
  struct tm *act_time = localtime(&raw_time);
	if(act_time == NULL){
		perror("localtime");
		exit(EXIT_FAILURE);
	}

  // Riempimento del buffer secondo il formato definito
	if(proc == THROTTLE){
		if(sprintf(log_phrase, "%02d/%02d/%d %02d:%02d:%02d - AUMENTO 5\n",
				act_time->tm_mday, act_time->tm_mon, act_time->tm_year + 1900,
				act_time->tm_hour, act_time->tm_min, act_time->tm_sec) < 0){
			perror("sprintf");
			exit(EXIT_FAILURE);
		}
	} else if(proc == BRAKE) {
		if(sprintf(log_phrase, "%02d/%02d/%d %02d:%02d:%02d - FRENO 5\n",
				act_time->tm_mday, act_time->tm_mon, act_time->tm_year + 1900,
				act_time->tm_hour, act_time->tm_min, act_time->tm_sec) < 0){
			perror("sprintf");
			exit(EXIT_FAILURE);
		}
	}
	else{
		perror("unknown proc type");
		exit(EXIT_FAILURE);
	}

  // Scrittura nel log file
	if(write(log_fd, log_phrase, size) < 0){
		perror("write");
		exit(EXIT_FAILURE);
	}
  free(log_phrase);
}

pid_t make_process(char *program_name, int name_length) {
	pid_t pid;
	char *program_path = malloc(name_length + 7);
	strcpy(program_path,"../bin/");
	program_path = strcat(program_path, program_name);
	if(!(pid = fork()))
		execl(program_path,program_name, NULL);
	else {
		free(program_path);
		return pid;
	}
}
