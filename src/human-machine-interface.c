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
#include <service-functions.h>


short int hmi_sock_fd;
short int comm_pipe[2];
char * input_buf;
char * output_buf;

void output_term();
void input_term();

int main() {

	initialize_socket("../tmp/hmi.sock", AF_UNIX, SOCK_STREAM, 5);
	pipe((int *) comm_pipe);

	if(!fork())
		input_term();
	if(!fork())
		output_term();


}

void input_term(){
	close (comm_pipe[READ]);

	dup2(comm_pipe[WRITE], STDOUT_FILENO);
	system("/usr/bin/gnome-terminal");
}

void output_term(){
	dup2(stdin)
	system("/usr/bin/gnome-terminal");
}


