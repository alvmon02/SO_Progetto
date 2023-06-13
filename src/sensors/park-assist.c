#include "../../include/service-functions.h"

int connect_to_ECU();
int read_from_ECU(int client_fd);

int main(int argc, char const *argv[])
{
	int client_fd, pid,
		park_assist_pipe_fd,
		ret = EXIT_SUCCESS;
		
	char* message_to_ECU;
	
	client_fd =  connect_to_ECU();
	
	while(!read_from_ECU(client_fd));
	
		//Creazione di un processo figlio per la gestione del surround-view-cameras
	pid = fork();
	if (pid != 0)
	{
			//Eliminazione di qualsiasi pipe con lo stesso nome
		unlink("assist-surround.pipe");
		
			//Creazione della pipe per la comunicazione con il surround-view-cameras
		park_assist_pipe_fd = initialize_pipe("assist-surround.pipe",O_RDONLY,0666);
		
			//Inizializazione dello eseguibile surround-view-cameras
		execl("../../bin/surround-view-cameras","surround-view-cameras",NULL);
		
			//Lettura della informazione dalla pipe
		read_output(park_assist_pipe_fd,message_to_ECU,SURR_CAM_LEN + 1);
		
			//Invio della informazione all'ECU attraverso UNIX_SOCKET
		broadcast_input(client_fd,message_to_ECU,SURR_CAM_LEN + 1);
		
		
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



/*

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
*/