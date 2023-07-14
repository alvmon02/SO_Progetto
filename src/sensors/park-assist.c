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


void signal_handler ( int );
int log_open();
FILE * pipe_open();

bool start_flag;
bool restart_flag;

int main(int argc, char *argv[]){

	struct sigaction act = { 0 };
	act.sa_flags = SA_RESTART;
	act.sa_handler = &signal_handler;
	sigaction(SIGUSR1, &act, NULL);
	sigaction(SIGUSR2, &act, NULL);
	sigaction(SIGINT, &act, NULL);

	if (argc != 2){
		perror("park-assist: syntax error");
		exit(EXIT_FAILURE);
	}

	start_flag = false;
	pid_t cameras_pid;
	int log_fd,
		cameras_fd,
		counter;
	FILE * assist_pipe;
	//Apertura del log file
	log_fd = log_open();

	char *hex_translation = malloc(BYTES_CONVERTED),
		 *mode = argv[1];

	int input_fd;
	if(!strcmp(mode, "ARTIFICIALE")) {
		input_fd= openat(AT_FDCWD, "urandomARTIFICIALE.binary", O_RDONLY);
		perror("park: open ARTIFICIALE");
	} else {
		input_fd = open("/dev/urandom", O_RDONLY);
		perror("park: open NORMALE");
	}

	//Creazione di un processo figlio per la gestione del surround-view-cameras
	cameras_pid = make_sensor(CAMERAS, mode, getpid());
	// Creazione della pipe per leggere la informazione
	// arrivata da bytes-sensor
	cameras_fd = initialize_pipe("tmp/cameras.pipe", O_RDONLY, 0666);

		//Connessione alla ECU
	assist_pipe = pipe_open();
	
	do
		sleep(1);
	while(!start_flag);
		

	while(true){
		restart_flag = false;
		counter = 0;
			// La Scrittura della informazione sulla pipe di comunicazione con la ECU
		// fatta nella funzione broad_log, anche con la scrittura della info
		// sullo file assist.log.
		// La Lettura della informazione dalla pipe
		// già fatta nella parte iniziale di read_conv_broad,
		// dato che cominzia con la lettura da park_assist,
		// e dopo si traduce il messagio, per il invio
		// a la ECU.

		while (counter++ < PARK_TIME && !restart_flag) {
			if(read_conv_broad(input_fd, hex_translation, assist_pipe->_fileno, log_fd) <= 0){
				lseek(input_fd, 0, SEEK_SET);
				perror("park: input file terminated: file pointer reset");
			}
			printf("Scrittura park numero: %d\n", counter);
			if(read(cameras_fd, hex_translation, BYTES_CONVERTED) < 0)
				perror("park: unable to read cameras pipe");
			else
				write(assist_pipe->_fileno, hex_translation, BYTES_CONVERTED);
			printf("Scrittura cameras numero: %d\n", counter);

		}
		if(restart_flag){
			perror("park: incomplete park: restarting cycle");
			continue;
		}
		kill(cameras_pid, SIGTSTP);
		sleep(2);
		printf("Pippo\n");
		if(kill(getppid(), SIGUSR2) < 0)
			perror("park: signaling ecu finished to write");
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

FILE * pipe_open(){
	FILE * pipe;
	while((pipe = fopen("./tmp/assist.pipe", "w")) == NULL )
		sleep(1);
	perror("park: pipe CONNECTED");
	return pipe;
}

void signal_handler(int sig){
	if(sig == SIGUSR1)
		start_flag = true;
	else if(sig == SIGUSR2)
		restart_flag = true;
	else if(sig == SIGINT) {
		kill(0, SIGKILL);
		exit(EXIT_SUCCESS);
	}

}
