#include <error.h>  /*per funzione perror*/
#include <fcntl.h>  /*per open*/
#include <signal.h> /*per la funzione kill*/
#include <stdio.h>  /*per sprintf, getline */
#include <stdlib.h> /*per funzione exit*/
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>  /*per mknod (sys call per mkfifo)*/
#include <sys/types.h> /*per mkfifo*/
#include <time.h> /*per le funzioni e le strutture relative a time, vedi
log( ) */
#include <unistd.h> /*per sistem calls e pipe*/
#include <sys/un.h>
#include "../../include/service-functions.h"

#define INPUT_LEN 8
#define PARK_TIME 30

short int log_fd;
short int assist_sock_fd;
short int cameras_pipe;
short int input_fd;

void launch_cameras( );
void broadcast_log_func();
void broa_log_writes(char []);

int main() { // COMMENTI DA CAMBIARE TUTTI!

	launch_cameras();
	cameras_pipe = open("../tmp/cameras.pipe", O_RDONLY);

	log_fd = open("../log/assist.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
	input_fd = open("../dev/urandom", O_RDONLY, 0400);
	assist_sock_fd = initialize_socket("../tmp/assist.sock", AF_UNIX,
SOCK_STREAM, 1);
	struct sockaddr_un ECU_addr;
	socklen_t ECU_addr_len;

	while (1) {
		if (accept(assist_sock_fd, (struct sockaddr*) &ECU_addr, &ECU_addr_len)) {
			for(int i = 0; i < PARK_TIME; i++){
				broadcast_log_func();;
				sleep(1);
			}
		}
	}
}

void launch_cameras( ){
	system("./surround-view-cameras");
}

void broadcast_log_func(){
	char buffer[INPUT_LEN];
		if(read (input_fd, &buffer, INPUT_LEN) == INPUT_LEN)
			broa_log_writes(buffer);
		if(read (input_fd, &buffer, INPUT_LEN) == INPUT_LEN)
			broa_log_writes(buffer);
}

void broa_log_writes(char message[]){
	write(assist_sock_fd, &message, INPUT_LEN);
	write(log_fd, &message, INPUT_LEN);
}
