#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/service-functions.h"

#define READ 0
#define WRITE 1

int steer_init ( );
void turn ( int , char * );
void throttle_failure_handler (int);
int arrest( );

struct process {
	pid_t pid;
	pid_t pgid;
	int pipe_fd;
};

int speed = 0;

int main(){
	struct process brake_process = brake_init();
	int steer_pipe_fd = steer_init(brake_process.pgid);
	int throttle_pipe_fd = throttle_init(brake_process.pgid);
	// c'e' scritto che durante il parcheggio ignora i vari sensori, non che li uccide come gli attuatori
	int camera_pipe_fd = camera_init();
	int radar_pipe_fd = radar_init();
	short int *hmi_fd = malloc(2*sizeof(short int));
	int log_fd = open("../log/ECU.log", O_WRONLY | O_APPEND | O_CREAT, 0644);

	// ciclo d'attesa per l'inizio del viaggio
	char *hmi_buf = malloc(11*sizeof(char));
	do {
		// qui ci vuole un minimo tempo d'attesa
		read(hmi_fd[READ], hmi_buf, 11);
	} while (strcmp(hmi_buf,"INIZIO"));

	// attivo il signal handler per l'errore nell'accelerazione
	signal(SIGUSR1, throttle_failure_handler);

	char *camera_buf = malloc(sizeof(char));
	char *radar_buf = malloc(8);
	pid_t parking_signal = -brake_process.pgid;
	int v = speed;
	int temp_v;
	while(TRUE) {
		read(radar_pipe_fd, radar_buf, 8);
		// QUI ORA DEVO LEGGERE DA HMI E AGIRE DI CONSEGUENZA
		read(hmi_fd[READ], hmi_buf, 11);
		if(!strcmp(hmi_buf, "PARCHEGGIO"))
			break;
		else if(!strcmp(hmi_buf, "ARRESTO"))
			arrest(brake_process.pid);
		
		// LEGGE DA FRONT WINDSHIELD CAMERA E AGISCE DI CONSEGUENZA
		read(camera_pipe_fd, camera_buf, 11);
		if(!strcmp(camera_buf, "PARCHEGGIO"))
			break;
		else if(!strcmp(camera_buf, "PERICOLO"))
			arrest(brake_process.pid);
		else if(!(strcmp(camera_buf, "DESTRA") && strcmp(camera_buffer, "SINISTRA")))
			send_command(steer_pipe_fd, log_fd, hmi_fd, camera_buf, sizeof(camera_buf));
		else if((temp_v = atoi(camera)) != 0)
			v = temp_v;
		
		// aggiorna la velocita' della macchina
		if(speed != v)
			change_speed(requested_speed, throttle_pipe_fd, brake_pipe_fd, log_fd, hmi_fd);
		// delay(400); ?? andra' bene come delay?? NOTA: sleep(int seconds)
	}

	// FRENATA PRIMA DI ATTIVARE LA PROCEDURA DI PARCHEGGIO
	while(speed != 0) {
		change_speed(0, throttle_pipe_fd, brake_pipe_fd, log_fd, hmi_fd);
		sleep(1);
	}

	// PROCEDURA DI PARCHEGGIO
	kill(parking_signal, SIGSTOP);
	int park_socket = park_assist_init();
	long int park_data;
	bool parking_completed = FALSE;
	while (!parking_completed) {
		int time = 0;
		while(time < 30) {
			read(park_socket, park_data, sizeof(long int));
			if(park_data == 0x172A || park_data == 0xD693 || park_data == 0x0 || park_data == 0xBDD8 || park_data == 0xFAEE || park_data == 0x4300)
				break;
			time++;
			if(time == 30)
				parking_completed = TRUE;
		}
	}
	return 0;
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
		execl("../bin/steer-by-wire","steer-by-wire",  NULL);
	else{
		int pipe_fd = initialize_pipe("../tmp/steer.pipe", O_WRONLY, 0660);
		setpgid(steer_pid, actuator_group);
		return pipe_fd;
	}
}

int throttle_init (pid_t actuator_group) {
	pid_t throttle_pid;
	if(!(throttle_pid = fork()))
		execl("../bin/throttle-control", NULL);
	else {
		int pipe_fd = initialize_pipe("../tmp/throttle.pipe", O_WRONLY, 0660);
		setpgid(throttle_pid, actuator_group);
		return pipe_fd;
	}
}

int camera_init () {
	if(!fork())
		execl("./windshield-camera", NULL);
	else
		return initialize_pipe("../tmp/camera.pipe", O_RDONLY, 660);
}

int radar_init () {
	if(!fork())
		execl("./forward-facing-radar", NULL);
	else
		return initialize_pipe("../tmp/radar.pipe", O_RDONLY, 660);
}

void hmi_init (pid_t *hmi_pipe_fd, int pipe_number) {
	system("../hmi-input");
	system("../hmi-output");
	hmi_fd[READ] = initialize_pipe("../tmp/hmi-input.pipe", O_RDONLY, 0660);
	hmi_fd[WRITE] = initialize_pipe("../tmp/hmi-output.pipe", O_WRONLY, 0660);
	return;
}

int park_assist_init() {
	if(!fork())
		execl("./park-assist", NULL);
	else
		return initialize_socket("../tmp/assist.sock", AF_UNIX, SOCK_STREAM, 5);
}

// INUTILE!!! HO GIA' UN'ALTRA FUNZIONE MIGLIORE
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

void change_speed(int requested_speed, int throttle_pipe_fd, int brake_pipe_fd, int log_fd, int hmi_fd) {
	if(requested_speed > speed) {
		send_command(throttle_pipe_fd, log_fd, hmi_fd, "INCREMENTO 5", sizeof("INCREMENTO 5"));
		speed += 5;
	} else if(requested_speed < speed) {
		send_command(brake_pipe_fd, log_fd, hmi_fd, "FRENO 5", sizeof("FRENO 5"));
		speed -= 5;
	}
}
