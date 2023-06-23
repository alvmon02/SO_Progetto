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

#define PARK_TIME 30

short int start;

void signal_start_handler(int sig);
void files_opening(int* log_assist_fd, int* bin_file_fd, char* bin_file_path);


int main(int argc, char const *argv[])
{
	start = 0;
	signal(SIGUSR2,signal_start_handler);
	while(!start);
	
	int pid,
		log_assist_fd, bin_file_fd,
		park_assist_pipe_fd, ECU_pipe_fd,
		counter = 0;
		
	char *message_to_ECU,
		 *aux_hex,
		 *system_random = "/dev/urandom",
		 *artificial_random = "../../urandomARTIFICIALE.binary";
	
	unsigned char *read_aux;
	
		//Connessione alla ECU
	ECU_pipe_fd = open("../tmp/assist.pipe",O_WRONLY);
	if (ECU_pipe_fd == -1){
		perror("Errore nell'apertura della pipe");
		exit(EXIT_FAILURE);
	}
	
		//Apertura dei file di log e binario
	files_opening(&log_assist_fd, &bin_file_fd,system_random);
	
		//Creazione di un processo figlio per la gestione del surround-view-cameras
	pid = fork();
	if (pid != 0)
	{
			//Eliminazione di qualsiasi pipe con lo stesso nome
		unlink("../tmp/cameras.pipe");
		
			//Creazione della pipe per la comunicazione con il surround-view-cameras
		park_assist_pipe_fd = initialize_pipe("cameras.pipe",O_RDONLY,0666);
		
			//Inizializazione dello eseguibile surround-view-cameras
		execl("../../bin/bytes-sensors","bytes-sensors",ASSIST,NULL);
		
		while (counter < PARK_TIME)
		{
				//Lettura della informazione dalla pipe
			read(park_assist_pipe_fd,message_to_ECU,SURR_CAM_LEN + 1);
			
				//Scrittura della informazione sulla pipe di comunicazione con la ECU
			write(ECU_pipe_fd,message_to_ECU,SURR_CAM_LEN + 1);
			
				/**
				 * Lettura della informazione dal file binario e invio
				 * della stessa allo log file, mentre che contemporaneamente
				 * si invia la informazione alla ECU
				 */
			read_conv_broad(bin_file_fd,read_aux,aux_hex,ECU_pipe_fd,log_assist_fd);
			sleep(1);
			counter++;
		}
		kill(pid,SIGTSTP);
	
		if(pid == 0 && counter == PARK_TIME)
			kill(getppid(),SIGUSR2);
	}	

	
	close(bin_file_fd);
	close(log_assist_fd);
	close(park_assist_pipe_fd);
	
	return EXIT_SUCCESS;
}

void files_opening(int* log_assist_fd, int* bin_file_fd, char* bin_file_path)
{
		//Apertura del logfile
	*log_assist_fd = open("../../log/assist.log",O_WRONLY | O_APPEND | O_CREAT,0666);
	if (*log_assist_fd == -1){
		perror("Errore nell'apertura del file di log");
		exit(EXIT_FAILURE);
	}
	
		//Apertura del file binario
	*bin_file_fd = open(bin_file_path,O_RDONLY);
	if (*bin_file_fd == -1){
		perror("Errore nell'apertura del file binario");
		exit(EXIT_FAILURE);
	}
}

void signal_start_handler(int sig){
	start = 1;
}