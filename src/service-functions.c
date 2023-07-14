/*Inclusione delle librerie necessarie*/
#define _POSIX_C_SOURCE 200809L

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
#include <errno.h>
#include "../include/service-functions.h"

// Funzione per l'inizializzazione delle pipe.
// La presente funzione esegue la parte di inizializzazione delle pipe.
// Questa viene eseguita da uno solo dei processi che si connettono alla pipe,
// infatti esegue la funzione unlink, la funzione mkfifo e la open con flags specificati come parametro.
int initialize_pipe(char * pipe_pathname, int flags, mode_t mode){
	unlink(pipe_pathname);
	if(mkfifoat(AT_FDCWD, pipe_pathname, mode) < 0)
		perror("init_pipe: mkfifoat pipe");
	int pipe_fd;
	while((pipe_fd = openat(AT_FDCWD, pipe_pathname, flags)) < 0){
		perror("init_pipe: openat pipe");
		sleep(1);
	}
	printf("Pipe %s inizialized\n", pipe_pathname);
	return pipe_fd;
}

// Funzione che converte una stringa binaria di bytes (unsigned char *)
// // in codifica esadecimale.
void hex ( unsigned char* to_conv, size_t size_to_conv, char* converted){
	for (int i = 0; i < size_to_conv; i++){
		converted[2*i] = to_conv[i]/16 + 48;
		converted[(2*i) + 1] = to_conv[i]%16 + 48;
		if(converted[2*i] > 57)
			converted[2*i] += 7;
		if(converted[(2*i) + 1] > 57)
			converted[(2*i) + 1] += 7;
	}
	converted[BYTES_CONVERTED] = '\0';
}

// Funzione per l'esecuzione sequenziale di invio tramite pipe di un messaggio
// e la scrittura dello stesso nel log file.
void broad_log (int pipe_fd, int log_fd, char * message, size_t size){
	if(write (pipe_fd, message, size) < 0  || write (log_fd, message, size) < 0 ){
		perror("broad_log: write");
	}
}

// Funzione utilizzata da byte-sensors.c e park-assist.c.
// La funzione esegue la lettura dei bytes binari dal sorgente in input_fd e li invia, se sono esattamente
// 8 bytes, tramite il canale di comunicazione comm_fd e li scrive nel file di log rappresentato
// dal file descriptor log_fd. Sfrutta le funzioni hex(unsigned char *, size_t, char *) e la funzione
// broad_log(int, int, char *, size_t)
int read_conv_broad(int input_fd, char * converted, int comm_fd, int log_fd){
	unsigned char * input_str = malloc(BYTES_LEN);
	int nread;
	if((nread = read(input_fd, input_str, BYTES_LEN)) == BYTES_LEN){
			hex(input_str, BYTES_LEN, converted);
			converted[BYTES_CONVERTED - 1] = '\n';
			broad_log(comm_fd, log_fd, converted, BYTES_CONVERTED);
	}
	free(input_str);
	sleep(1);
	return nread;
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
    perror("time log: malloc");
    exit(EXIT_FAILURE);
  }

  /* Dichiarazione di raw_time, un tempo aritmetico utile per la costruzione di
   * act_time, e inizializzazione di questo al tempo aritmetico relativo al
   * momento attuale di esecuzione time(NULL)*/
	time_t raw_time = time(NULL);
	if(raw_time == (time_t) -1){
		perror("time log: time");
		exit(EXIT_FAILURE);
	}

  // Struttura di time.h che mantiene campi distinti per ogni campo della data
  // e dell'orario
  struct tm *act_time = localtime(&raw_time);
	if(act_time == NULL){
		perror("time log: localtime");
		exit(EXIT_FAILURE);
	}

  // Riempimento del buffer secondo il formato definito
	if(proc == THROTTLE){
		if(sprintf(log_phrase, "%02d/%02d/%d %02d:%02d:%02d - AUMENTO 5\n",
				act_time->tm_mday, act_time->tm_mon, act_time->tm_year + 1900,
				act_time->tm_hour, act_time->tm_min, act_time->tm_sec) < 0){
			perror("time log: sprintf");
			exit(EXIT_FAILURE);
		}
	} else if(proc == BRAKE) {
		if(sprintf(log_phrase, "%02d/%02d/%d %02d:%02d:%02d - FRENO 5\n",
				act_time->tm_mday, act_time->tm_mon, act_time->tm_year + 1900,
				act_time->tm_hour, act_time->tm_min, act_time->tm_sec) < 0){
			perror("time log: sprintf");
			exit(EXIT_FAILURE);
		}
	}
	else{
		perror("time log: unknown proc type");
		exit(EXIT_FAILURE);
	}

  // Scrittura nel log file
	if(write(log_fd, log_phrase, size) < 0){
		perror("time log: write");
		exit(EXIT_FAILURE);
	}
  free(log_phrase);
}
pid_t make_process(char *program_name, int name_length, pid_t pgid, char *args) {
	pid_t pid;
	char *program_path = malloc(name_length + 6);
	if(program_path == NULL)
		perror("make process: malloc");
	strcpy(program_path,"./bin/");
	strcat(program_path, program_name);
	pid = fork();
	if(pid < 0)
		perror("make process: fork");
	else if(pid == 0){
		setpgid(0, pgid);
		if(args != NULL)
			execlp(program_path,program_name, args, NULL);
		else
			execlp(program_path,program_name, NULL);
	}
	free(program_path);
	return pid;
}

pid_t make_sensor(char *program_name, char *mode, pid_t pgid) {
	pid_t pid;
	if(!(pid = fork())){
		setpgid(0, pgid);
		if(execlp("./bin/bytes-sensors","bytes-sensors", mode, program_name, NULL) < 0)
			perror("make_sensor: execlp");
	}
	return pid;
}
