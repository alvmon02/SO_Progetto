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


void signal_start_handler ( int );
void restart_handler ( int );
int log_open();
int pipe_open();

bool start_flag;
bool restart_flag;

int main(int argc, char *argv[]){

	if (argc != 2){
		perror("park-assist: syntax error");
		exit(EXIT_FAILURE);
	}

	start_flag = false;
	pid_t cameras_pid;
	int log_fd,
		cameras_fd,
		assist_fd,
		counter = 0;

	//Apertura del log file
	log_fd = log_open();

	unsigned char *bytes_read = malloc(BYTES_LEN);
	char *hex_translation = malloc(BYTES_CONVERTED),
		*mode = argv[1];

	int input_fd;
	if(strcmp(mode, ARTIFICIALE))
		input_fd= openat(AT_FDCWD, "urandomARTIFICIALE.binary", O_RDONLY);
	else
		input_fd = open("/dev/urandom", O_RDONLY);

	//Creazione di un processo figlio per la gestione del surround-view-cameras
	cameras_pid = make_sensor(CAMERAS, mode);
	// Creazione della pipe per leggere la informazione
	// arrivata da bytes-sensor
	cameras_fd = initialize_pipe("tmp/cameras.pipe", O_RDONLY, 0666);

		//Connessione alla ECU
	assist_fd = pipe_open();

	signal(SIGUSR1,signal_start_handler);
	signal(SIGUSR2, restart_handler);

	while(!start_flag)
		sleep(1);

	while(true){
		restart_flag = false;
		while (counter++ < PARK_TIME) {
			// La Scrittura della informazione sulla pipe di comunicazione con la ECU
			// fatta nella funzione broad_log, anche con la scrittura della info
			// sullo file assist.log.
			// La Lettura della informazione dalla pipe
			// già fatta nella parte iniziale di read_conv_broad,
			// dato che cominzia con la lettura da park_assist,
			// e dopo si traduce il messagio, per il invio
			// a la ECU.

			read_conv_broad(input_fd, bytes_read, hex_translation, assist_fd, log_fd);
			perror("park assist: read_borad_log");

			read(cameras_fd, bytes_read, BYTES_LEN);
			hex(bytes_read, BYTES_LEN, hex_translation);
			write(assist_fd, hex_translation, BYTES_CONVERTED);
			perror("park assist: cameras transmit");

		}
		kill(getppid(), SIGUSR2);
		perror("park: kill parent usr2");
		kill(cameras_pid,SIGTSTP);
		while(!restart_flag)
			sleep(1);
		kill(cameras_pid, SIGCONT);
	}
}

int log_open(){
	int fd;
		//Apertura del logfile
	fd = openat(AT_FDCWD, "log/assist.log",O_WRONLY | O_TRUNC | O_CREAT,0666);
	if (fd < 0){
		perror("park assist: openat log");
		exit(EXIT_FAILURE);
	}
	return fd;
}

int pipe_open(){
	int fd;
	while((fd = openat(AT_FDCWD, "tmp/assist.pipe", O_WRONLY | O_NONBLOCK, 0666)) < 0 )
		sleep(1);
	perror("park: pipe CONNECTED");
	return fd;
}

void signal_start_handler(int sig){
	start_flag = true;
}

void restart_handler (int sig){
	restart_flag = true;
}
