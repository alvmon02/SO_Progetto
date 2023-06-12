// #include <error.h>  /*per funzione perror*/
// #include <fcntl.h>  /*per open*/
// #include <signal.h> /*per la funzione kill*/
// #include <stdio.h>  /*per sprintf, getline */
// #include <stdlib.h> /*per funzione exit*/
// #include <string.h>
// #include <sys/socket.h>
// #include <sys/stat.h>  /*per mknod (sys call per mkfifo)*/
// #include <sys/types.h> /*per mkfifo*/
// #include <time.h> /*per le funzioni e le strutture relative a time, vedi
// log( ) */
// #include <unistd.h> /*per sistem calls e pipe*/
//
// #define INPUT_LEN 8
//
// short int log_fd;
// short int serv_sock_fd;
//
// void broadcast_input(char *, int);
// void log_func(char *, int);
//
// int main() { // COMMENTI DA CAMBIARE TUTTI!
//
// 	unlink("../tmp/assist.sock");
// 	serv_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
// 	struct sockaddr_un serv_addr, cli_addr;
// 	int cli_addr_len;
// 	serv_addr.sun_family = AF_UNIX;
// 	strcpy(serv_addr.sun_path, "../tmp/assist.sock");
// 	if (bind(serv_sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
// 		perror("bind");
// 		exit(EXIT_FAILURE);
// 	}
// 	if (listen (serv_sock_fd, 1)){
// 		perror("listen");
// 		exit(EXIT_FAILURE);
// 	}
//
// 	log_fd = open("../log/assist.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
// 	short int input_fd = open("../dev/urandom", O_RDONLY, 0400);
//
// 	char assist_input[INPUT_LEN];
//
// 	while (1) {
// 		if (connect(serv_sock_fd, &cli_addr, &cli_addr_len)) {
// 			for(int i = 0, i < PARK_TIME, i++){
// 				broadcast_input(assist_input, INPUT_LEN);
// 				log_func(assist_input, INPUT_LEN);
// 				sleep(1);
// 			}
// 		} else
// 			sleep(1);	/*si potrebbe anche togliere in realta'*/
// 	}
// }
//
// void broadcast_input(char *message, int size) {
// 	write(pipe_fd, &message, INPUT_LEN);
// 	return;
// }
//
// void log_func(char *log_phrase, int size) {
// 	write(log_fd, log_phrase, INPUT_LEN);
// 	return;
// }
