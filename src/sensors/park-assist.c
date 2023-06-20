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
#include "../../include/service-functions.h"

#define PARK_TIME 30

int connect_to_ECU();
int read_from_ECU(int client_fd);

int main(int argc, char const *argv[])
{
	int client_fd, pid,
		park_assist_pipe_fd,
		counter = 0,
		ret = EXIT_SUCCESS;
		
	char* message_to_ECU;
	
	client_fd =  connect_to_ECU();
	
	while(!read_from_ECU(client_fd));
	
		//Creazione di un processo figlio per la gestione del surround-view-cameras
	pid = fork();
	if (pid != 0)
	{
			//Eliminazione di qualsiasi pipe con lo stesso nome
		unlink("../tmp/cameras.pipe");
		
			//Creazione della pipe per la comunicazione con il surround-view-cameras
		park_assist_pipe_fd = initialize_pipe("assist-surround.pipe",O_RDONLY,0666);
		
			//Inizializazione dello eseguibile surround-view-cameras
		execl("../../bin/surround-view-cameras","surround-view-cameras",NULL);
		
		while (counter < PARK_TIME)
		{
				//Lettura della informazione dalla pipe
			read_output(park_assist_pipe_fd,message_to_ECU,SURR_CAM_LEN + 1);
		
				//Invio della informazione all'ECU attraverso UNIX_SOCKET
			broadcast_input(client_fd,message_to_ECU,SURR_CAM_LEN + 1);
			
			wait(1);
			counter++;
		}
		
	}
	else
		ret = EXIT_FAILURE;
	
		//Eliminazione di qualsiasi pipe con lo stesso nome
	unlink("assist-surround.pipe");

	close(client_fd);
	close(park_assist_pipe_fd);
	
	return ret;
}


int connect_to_ECU()
{
	int server_len,client_fd,
		socket_connected = 0,
		counter = 0;
	
	struct sockaddr_un server_addr;
	struct sockaddr* server_addr_ptr;
	
	server_addr_ptr = (struct sockaddr*)& server_addr;
	server_len = sizeof(server_addr);
	
	client_fd = socket(AF_UNIX,SOCK_STREAM,0);
	
	server_addr.sun_family = AF_UNIX;
	
	strcpy(server_addr.sun_path,"../../tmp/assist.sock");
	
	//As signal is given by a shared value
	
	while(!socket_connected){
		socket_connected = connect(client_fd,server_addr_ptr,server_len);
		if(socket_connected == -1){
			
				//DEBUG only
			perror("connect Error");
			exit(EXIT_FAILURE);
		}
	}
	
	return client_fd;
}

int read_from_ECU(int client_fd)
{
	int start_signal = 0;
	char* message_from_ECU;
	
	read_output(client_fd,message_from_ECU,128);


	if(strcmp(message_from_ECU,"START") == 0)
		start_signal = 1;

	return start_signal;
}