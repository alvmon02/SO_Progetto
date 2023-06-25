#define _POSIX_C_SOURCE 200809L

/*Inclusione delle librerie necessarie*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdbool.h>
#include "../../include/service-functions.h"


void signal_start_handler(int sig);
int log_open();
int assist_pipe();

bool start;
bool restart_flag;

int main(int argc, char *argv[])
{
	start = false;
	int pid,
		log_assist_fd,
		cameras_fd,
		assist_fd,
		counter = 0;

	signal(SIGUSR2,signal_start_handler);

	while(!start)
		sleep(1);

	unsigned char *bytes_read = malloc(BYTES_LEN);
	char *hex_translation = malloc(BYTES_CONVERTED),
		*mode = argv[1];
	
	//Connessione alla ECU
	assist_fd = assist_pipe();
	
	//Apertura del log file
	log_assist_fd = log_open();

	int input_fd = openat(AT_FDCWD, (strcmp(mode, ARTIFICIALE)? "urandomARTIFICIALE.binary" : "/dev/urandom"), O_RDONLY);

	//Creazione di un processo figlio per la gestione del surround-view-cameras
	pid = make_sensor(CAMERAS, mode);
	// Creazione della pipe per leggere la informazione
	// arrivata da bytes-sensor
	cameras_fd = initialize_pipe("tmp/cameras.pipe", O_RDONLY, 0666);

	while(true){
		restart_flag = false;
		while (counter++ < PARK_TIME && restart_flag) {
			// La Scrittura della informazione sulla pipe di comunicazione con la ECU
			// fatta nella funzione broad_log, anche con la scrittura della info
			// sullo file assist.log.
			// La Lettura della informazione dalla pipe
			// già fatta nella parte iniziale di read_conv_broad,
			// dato che cominzia con la lettura da park_assist,
			// e dopo si traduce il messagio, per il invio
			// a la ECU.
			read_conv_broad(input_fd, bytes_read, hex_translation, assist_fd, log_assist_fd);

			read(cameras_fd, bytes_read, BYTES_LEN);
			hex(bytes_read, BYTES_LEN, hex_translation);
			write(assist_fd, hex_translation, BYTES_CONVERTED);

			sleep(1);
		}
		kill(pid,SIGTSTP);
		while(!restart_flag)
			sleep(1);
	}
}

int log_open(){
	int fd;
		//Apertura del logfile
	fd = openat(AT_FDCWD, "log/assist.log",O_WRONLY | O_APPEND | O_CREAT,0666);
	if (fd < 0){
		perror("park assist: openat log");
		exit(EXIT_FAILURE);
	}
	return fd;
}

int assist_pipe(){
	int fd;
	while((fd = openat(AT_FDCWD, "tmp/assist.pipe",O_WRONLY | O_TRUNC | O_CREAT,0666) < 0));
	perror("park assist: CONNECTED");
	return fd;
}

void signal_start_handler(int sig){
	start = true;
}

void restart_handler (int sig){
	restart_flag = true;
}
