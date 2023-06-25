#define _POSIX_C_SOURCE 200809L
/*Inclusione delle librerie necessarie*/
#include <stdio.h>  /*per sprintf, getline */
#include <stdlib.h> /*per funzione exit*/
#include <fcntl.h>  /*per open*/
#include <unistd.h> /*per sistem calls e pipe*/
#include <errno.h>  /*per funzione perror*/
#include <string.h> /*per funzione strlen*/
#include <ctype.h> /*per la funzione toupper*/
#include <sys/stat.h>  /*per mknod (sys call per mkfifo)*/
#include <signal.h>  /*per il uso di segnali*/
#include "../../include/service-functions.h"

short int start;

void signal_start_handler(int sig);
int log_open();
int ECU_pipe();


int main(int argc, char *argv[])
{
	start = 0;
	signal(SIGUSR2,signal_start_handler);
	while(!start);
	
	int pid,
		log_assist_fd, bin_file_fd,
		park_assist_pipe_fd, ECU_pipe_fd,
		counter = 0;
		
	char *message_to_ECU,
		 *hex_translation,
		 *mode = argv[1];
	
	unsigned char *bytes_read;
	
		//Connessione alla ECU
	ECU_pipe_fd = ECU_pipe();
	
		//Apertura del log file
	log_assist_fd = log_open();
	
		//Creazione di un processo figlio per la gestione del surround-view-cameras
	pid = make_sensor(CAMERAS,mode);
	if (pid > 0){
		
		// Creazione della pipe per leggere la informazione
		// arrivata da bytes-sensor
		park_assist_pipe_fd = initialize_pipe("./tmp/cameras.pipe",O_CREAT | O_WRONLY,0666);		
		
		while (counter < PARK_TIME)
		{
			// La Scrittura della informazione sulla pipe di comunicazione con la ECU
			// fatta nella funzione broad_log, anche con la scrittura della info
			// sullo file assist.log.
			// La Lettura della informazione dalla pipe
			// già fatta nella parte iniziale di read_conv_broad,
			// dato che cominzia con la lettura da park_assist,
			// e dopo si traduce il messagio, per il invio
			// a la ECU.
			read_conv_broad(park_assist_pipe_fd,bytes_read,hex_translation,ECU_pipe_fd,log_assist_fd);
			
			sleep(1);
			counter++;
		}
		kill(pid,SIGTSTP);
	
		if(pid <= 0 && counter == PARK_TIME)
			kill(getppid(),SIGUSR2);
	}	

	
	close(bin_file_fd);
	close(log_assist_fd);
	close(park_assist_pipe_fd);
	
	return EXIT_SUCCESS;
}

int log_open(){
	int fd = -1;
		//Apertura del logfile
	fd = open("./log/assist.log",O_WRONLY | O_APPEND | O_CREAT,0666);
	if (fd == -1){
		perror("Errore nell'apertura del file di log");
		exit(EXIT_FAILURE);
	}
	
	return fd;
}

int ECU_pipe(){
	int fd = -1;
	
	fd = open("./tmp/assist.pipe",O_WRONLY | O_APPEND | O_CREAT,0666);
	if (fd == -1){
		perror("Errore nell'apertura della pipe");
		exit(EXIT_FAILURE);
	}
	
	return fd;
}

void signal_start_handler(int sig){
	start = 1;
}
