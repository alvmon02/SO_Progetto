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

struct process {
	pid_t pid;
	pid_t pgid;
	int pipe_fd;
};

int main(){
	int speed; // credo dovrebbe essere global o static

	struct process brake_process = brake_init();
	int steer_pipe_fd = steer_init(brake_process.pgid);
	int throttle_pipe_fd = throttle_init(brake_process.pgid);
	// c'e' scritto che durante il parcheggio ignora i vari sensori, non che li uccide come gli attuatori
	int camera_pipe_fd = camera_init();
	int radar_pipe_fd = radar_init();
	short int *hmi_fd = malloc(2*sizeof(short int));

	// ciclo d'attesa per l'inizio del viaggio
	char *hmi_buffer = malloc(11*sizeof(char));
	do {
		// qui ci vuole un minimo tempo d'attesa
		read(hmi_fd[READ], hmi_buffer, 11);
	} while (strcmp(hmi_buffer,"INIZIO"));

	// attivo il signal handler per l'errore nell'accelerazione
	signal(SIGUSR1, throttle_failure_handler);

	char *camera_buf = malloc(sizeof(char));
	char *radar_buf = malloc(8);
	while(TRUE) {
		read(radar_pipe_fd, radar_buf, 8);
		// QUI ORA DEVO LEGGERE DA HMI E FRONT WINDSHIELD CAMERA E AGIRE DI CONSEGUENZA
	}
}

struct process brake_init () {
	pid_t brake_pid;
	if((brake_pid = fork()) == 0)
		execl("./brake-by-wire", NULL);
	else {
		struct process brake_process;
		process brake_process.pid = brake_pid;
		process brake_process.pgid = getpgid(brake_pid);
		process brake_process.pipe_fd = initialize_pipe("../temp/brake.pipe", O_WRONLY, 660);
		return brake_process;
	}
}

int steer_init (pid_t actuator_group) {
	pid_t steer_pid;
	if(!(steer_pid = fork()))
		execl("./steer-by-wire", NULL);
	else{
		int pipe_fd = initialize_pipe("../temp/steer.pipe", O_WRONLY, 0660);
		setpgid(steer_pid, actuator_group);
		return pipe_fd;
	}
}

int throttle_init (pid_t actuator_group) {
	pid_t throttle_pid;
	if(!(throttle_pid = fork()))
		execl("./throttle-control", NULL);
	else {
		int pipe_fd = initialize_pipe("../temp/throttle.pipe", O_WRONLY, 0660);
		setpgid(throttle_pid, actuator_group);
		return pipe_fd;
	}
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

void hmi_init (pid_t *hmi_pipe_fd, int pipe_number) {
	system("../hmi-input");
	system("../hmi-output");
	hmi_fd[READ] = initialize_pipe("../temp/hmi-input.pipe", O_RDONLY, 0660);
	hmi_fd[WRITE] = initialize_pipe("../temp/hmi-output.pipe", O_WRONLY, 0660);
	return;
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
	// poi devo uccidere i vari processi (tutti e poi se stesso)
}

void arrest(pid_t brake_process) {
	kill(brake_process, SIGUSR1);
	speed = 0;
}

void send_command(int actuator_pipe_fd, int log_fd, int hmi_fd, char *command, size_t command_size) {
	broad_log(actuator_pipe_fd, log_fd, command, command_size);
	write(hmi_fd, command, command_size);
}