/*Inclusione delle librerie necessarie*/
#include <stdio.h>  /*per sprintf, getline */
#include <stdlib.h> /*per funzione exit*/
#include <fcntl.h>  /*per open*/
#include <unistd.h> /*per sistem calls e pipe*/
#include <errno.h>  /*per funzione perror*/
#include <string.h> /*per funzione strlen*/
#include <ctype.h> /*per la funzione toupper*/
#include <sys/stat.h>  /*per mknod (sys call per mkfifo)*/
#include <sys/socket.h> /*per socket tipi*/
#include <sys/un.h> /*per la conessione UNIX_SOCKET*/
#include "../include/service-functions.h"

short int initialize_pipe(char * pipe_pathname, int flags, mode_t mode){
	short int fd;
	unlink (pipe_pathname);
	mkfifo(pipe_pathname, mode);
	return (fd = open(pipe_pathname, flags));
}

// Funzione per la lettura di messaggi tramite pipe
void read_output (int fd, char * message_out, size_t size){
	read (fd, message_out, size);
}

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
	write (pipe_fd, message, size);
	write (log_fd,	message, size);
}

// str_toupper restituise la stringa in input str come una stringa
// di caratteri maiuscoli. Sfrutta la funzione posix toupper(char).
char * str_toupper(char * str) {
	for(char * p = str; *p != 0; p++)
		*p = toupper(*p);
	return str;
}

pid_t make_process(char *program_name, int name_length, char *args) {
	pid_t pid;
	char *program_path = malloc(name_length + 7);
	strcpy(program_path,"../bin/");
	program_path = strcat(program_path, program_name);
	if(!(pid = fork())) {
		if(args != NULL)
			execl(program_path,program_name, args, NULL);
		else
			execl(program_path,program_name, NULL);
	}
	free(program_path);
	return pid;
}

pid_t make_sensor(char *program_name, char *mode) {
	pid_t pid;
	if(!(pid = fork()))
		execl("../bin/bytes_sensors","bytes_sensors", mode, program_name, NULL);
	else
		return pid;
}