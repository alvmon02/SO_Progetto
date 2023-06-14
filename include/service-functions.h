
#ifndef SERVICE_FUNCTIONS_H_DEFINED
#define SERVICE_FUNCTIONS_H_DEFINED

	/*Inclusione delle librerie necessarie*/
	#include <stdio.h>  /*per sprintf, getline */
	#include <stdlib.h> /*per funzione exit*/
	#include <signal.h> /*per la funzione kill*/
	#include <sys/types.h> /*per mkfifo*/
	#include <string.h> /*per funzione strlen*/
	#include <sys/socket.h> /*per socket tipi*/
	#include <error.h>  /*per funzione perror*/
	#include <sys/stat.h>  /*per mknod (sys call per mkfifo)*/
	#include <fcntl.h>  /*per open*/
	#include <unistd.h> /*per sistem calls e pipe*/
	#include <sys/un.h> /*per la conessione UNIX_SOCKET*/
	#include <ctype.h> /*per la funzione toupper*/
	#include <sys/types.h>



	#define READ 0
	#define WRITE 1
	#define NORMALE 0
	#define ARTIFICIALE 1
	#define RADAR 0
	#define CAMERAS 1
	#define ASSIST 2
	#define SURR_CAM_LEN 8

	short int initialize_socket(char * sock_pathname, int domain, int type, int queue_len);

	short int initialize_pipe(char * pipe_pathname, int flags, mode_t mode);

	char * hex (unsigned long long bytes);

	void bytes_log (int log_fd, unsigned long bytes, size_t size);

	void read_output (int pipe_fd, char * message_out, size_t size);

	void log_func (int log_fd, char * log_phrase, size_t size);

	void broad_log (int pipe_fd, int log_fd, char * message, size_t size);

	char * str_toupper(char * str);

#endif // SERVICE_FUNCTIONS_H_DEFINED
