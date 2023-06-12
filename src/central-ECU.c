#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define READ 0
#define WRITE 1

int steer_init ( );
void turn ( int , char * );
void throttle_failure_handler (int);
void arrest( );

int main(){
	int speed; // credo dovrebbe essere global o static
	int brake_process;
	signal(SIGUSR1, throttle_failure_handler);

	int steer_pipe_fd = steer_init();
	int throttle_pipe_fd = throttle_init();
	int brake_pipe_fd = brake_init(&brake_process);
	int camera_pipe_fd = camera_init();
	int radar_pipe_fd = radar_init();
	short int *hmi_fd = malloc(2*sizeof(short int));
	hmi_fd[READ] = initialize_pipe("../temp/hmi-input.pipe", O_RDONLY, 0660);
	hmi_fd[WRITE] = initialize_pipe("../temp/hmi-output.pipe", O_WRONLY, 0660);

}



int steer_init ( ) {
	if(!fork())
		execl("./steer-by-wire", NULL);
	else{
		int pipe_fd;
		while(pipe_fd = open ("../temp/steer.pipe", O_WRONLY) >= 0)
			sleep(1);
		return pipe_fd;
	}
}

int throttle_init ( ) {
	if(!fork())
		execl("./throttle-control", NULL);
	else
		return initialize_pipe("../temp/throttle.pipe", O_WRONLY, 660);
}

// questo pero' deve restituire anche il pid del processo creato per segnale d'arresto
int brake_init (pid_t *brake_process_addr) {
	if((*brake_process_addr = fork()) == 0)
		execl("./brake-by-wire", NULL);
	else
		return initialize_pipe("../temp/brake.pipe", O_WRONLY, 660);
}

int camera_init () {
	if(!fork())
		execl("./windshield-camera", NULL);
	else
		return initialize_pipe("../temp/camera.pipe", O_RDONLY, 660);
}

int radar_init () {
	if(!fork())
		execl("./forward-facing-radar", NULL);
	else
		return initialize_pipe("../temp/radar.pipe", O_RDONLY, 660);
}

int hmi_init () {
	pipe();
	system();
}

int park_assist_init() {
	if(!fork())
		execl("./park-assist", NULL);
	else
		return initialize_socket("../tmp/assist.sock", AF_UNIX, SOCK_STREAM, 5);
}

void turn (int steer_pipe_fd, char *direction ){
	write (steer_pipe_fd, direction, 9);
	return;
}

void throttle_failure_handler (int sig){
	arrest(brake_pid);
}

void arrest(pid_t brake_process) {
	kill(brake_process, SIGUSR1);
	speed = 0;
}

void send_command(int actuator_pipe_fd, int log_fd, int hmi_fd, char *command, size_t command_size) {
	broad_log(actuator_pipe_fd, log_fd, command, command_size);
	write(hmi_fd, command, command_size);
}