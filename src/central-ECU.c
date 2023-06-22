#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "../include/service-functions.h"

#define HMI_COMMAND_LENGTH 11 
#define RADAR_BYTES_NUMBER 8
#define PARK_BYTES_NUMBER 8
#define TERMINAL_NAME_MAX_LENGTH 50
#define MOD_LENGTH 12

struct pipe_process {
	pid_t pid;
	pid_t pgid;
	int pipe_fd;
} brake_process;

struct groups {
	pid_t park_assist_group;
	pid_t actuators_group;
	pid_t sensors_group;
	pid_t hmi_group;
} processes_groups;

struct pipe_process brake_init();
int steer_init(pid_t);
int throttle_init(pid_t);
struct pipe_process camera_init();
int radar_init(pid_t);
struct pipe_process park_assist_init();
void hmi_init(struct pipe_process *,int);
void arrest(pid_t);
void ECU_signal_handler(int);
void send_command(int, int, int, char *, size_t);
void change_speed(int, int, int, int, int);

int speed = 0;

int main(int argc, char **argv){

	if(argc < 2 || argc > 4 || argc == 3) {
		perror(" syntax error");
		exit(EXIT_FAILURE);
	}

	if(strcmp(argv[1],ARTIFICIALE) && (strcmp(argv[1],NORMALE))) {
		perror(" invalid input modality");
		exit(EXIT_FAILURE);
	} else
		char modalita[MOD_LENGTH] = argv[1];

	// imposta il terminale scelto dall'utente
	if(!(strcmp(argv[2], "--term"))) {
		if(!fork()) {
			char *term_command = malloc(TERMINAL_NAME_MAX_LENGTH);
			sprintf(term_command,"./sh/new_terminal.sh %s\0", argv[3]);
			execlp("/usr/bin/bash","bash","-c", term_command, NULL);
		}
	}

	unmask(000);
	int error_log_fd = open("../log/errors.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
	dup2(stderr, error_log_fd);

	brake_process = brake_init();
	int steer_pipe_fd = steer_init(brake_process.pgid);
	int throttle_pipe_fd = throttle_init(brake_process.pgid);
	processes_groups.actuators_group = brake_process.pgid;

	struct pipe_process camera_process = camera_init();
	pid_t sensor_signal = -camera_process.pgid;
	int radar_pipe_fd = radar_init(camera_process.pgid, modalita);
	processes_groups.sensors_group = camera_process.pgid;
	struct pipe_process *hmi_process = malloc(2*sizeof(struct pipe_process));
	hmi_init(hmi_process, 2);
	processes_groups.hmi_group = hmi_process->pgid;
	int log_fd = open("../log/ECU.log", O_WRONLY | O_APPEND | O_CREAT, 0644);

	bool travel_flag = true;

	// ciclo d'attesa per l'inizio del viaggio
	char *hmi_buf = malloc(HMI_COMMAND_LENGTH);
	do {
		// qui ci vuole un minimo tempo d'attesa
		read(hmi_process[READ].pipe_fd, hmi_buf, HMI_COMMAND_LENGTH);
		if(!(strcmp(hmi_buf,"PARCHEGGIO")))
			travel_flag = false;
	} while (strcmp(hmi_buf,"INIZIO") && strcmp(hmi_buf,"PARCHEGGIO"));

	// attivo il signal handler per l'errore nell'accelerazione
	signal(SIGUSR1, ECU_signal_handler);
	signal(SIGUSR2, ECU_signal_handler);

	char *camera_buf = malloc(HMI_COMMAND_LENGTH);
	char *radar_buf = malloc(RADAR_BYTES_NUMBER);
	pid_t parking_signal = -brake_process.pgid;
	int requested_speed = speed;
	int temp_v;
	while(travel_flag) {
		read(radar_pipe_fd, radar_buf, RADAR_BYTES_NUMBER);
		// LEGGE DA HMI E AGISCE DI CONSEGUENZA
		read(hmi_process[READ].pipe_fd, hmi_buf, HMI_COMMAND_LENGTH);
		if(!strcmp(hmi_buf, "PARCHEGGIO"))
			break;
		else if(!strcmp(hmi_buf, "ARRESTO"))
			arrest(brake_process.pid);
		
		// LEGGE DA FRONT WINDSHIELD CAMERA E AGISCE DI CONSEGUENZA
		read(camera_process.pipe_fd, camera_buf, HMI_COMMAND_LENGTH);
		if(!strcmp(camera_buf, "PARCHEGGIO"))
			break;
		else if(!strcmp(camera_buf, "PERICOLO"))
			arrest(brake_process.pid);
		else if(!(strcmp(camera_buf, "DESTRA") && strcmp(camera_buf, "SINISTRA")))
			send_command(steer_pipe_fd, log_fd, hmi_process[WRITE].pipe_fd, camera_buf, sizeof(camera_buf));
		else if((temp_v = atoi(camera_buf)) != 0)
			requested_speed = temp_v;
		
		// aggiorna la velocita' della macchina
		if(speed != requested_speed)
			change_speed(requested_speed, throttle_pipe_fd, brake_process.pipe_fd, log_fd, hmi_process[WRITE].pipe_fd);
		
		sleep(1);
	}

	// FRENATA PRIMA DI ATTIVARE LA PROCEDURA DI PARCHEGGIO
	while(speed != 0) {
		change_speed(0, throttle_pipe_fd, brake_process.pipe_fd, log_fd, hmi_process[WRITE].pipe_fd);
		sleep(1);
	}

	// PROCEDURA DI PARCHEGGIO
	kill(parking_signal, SIGINT);
	char park_data[PARK_BYTES_NUMBER];
	bool parking_completed = false;
	struct socket_process park_process;
	park_process = park_assist_init(modalita);
	processes_groups.park_assist_group = park_process.pgid;
	while (!parking_completed) {
		int time = 0;
		while(time < PARK_TIME) {
			read(park_process.pipe_fd, park_data, PARK_BYTES_NUMBER);
			if(accettable_string(park_data, PARK_BYTES_NUMBER))
				break;
			write(park_process.pipe_fd, CONTINUE, 1);
			if(++time == PARK_TIME)
				parking_completed = true;
			sleep(1);
		}
		if(!parking_completed)
			write(park_process.pipe_fd, RELOAD, 1);
	}

	// TERMINO I PROCESSI DEI SENSORI E QUELLI DEGLI ATTUATORI
	kill(park_process.pid, SIGKILL);
	kill(sensor_signal, SIGKILL);

	// QUI ORA UCCIDO LA HMI MA PRIMA VOGLIO MANDARE UN SEGNALE DIFFERENTE
	kill(-(processes_groups.hmi_group), SIGKILL);

	return 0;
}

struct pipe_process brake_init() {
	struct pipe_process brake_process;
	brake_process.pid = make_process("brake-by-wire", 14);
	brake_process.pgid = getpgid(brake_process.pid);
	brake_process.pipe_fd = initialize_pipe("../tmp/brake.pipe", O_WRONLY, 0664);
	return brake_process;
}

int steer_init(pid_t actuator_group) {
	int pipe_fd = initialize_pipe("../tmp/steer.pipe", O_WRONLY, 0664);
	setpgid(make_process("steer-by-wire",14), actuator_group);
	return pipe_fd;
}

int throttle_init(pid_t actuator_group) {
	int pipe_fd = initialize_pipe("../tmp/throttle.pipe", O_WRONLY, 0664);
	setpgid(make_process("throttle-control", 17), actuator_group);
	return pipe_fd;
}

struct pipe_process camera_init() {
	struct pipe_process camera_process;
	camera_process.pid = make_process("windshield-camera", 18);
	camera_process.pgid = getpgid(camera_process.pid);
	camera_process.pipe_fd = initialize_pipe("../tmp/camera.pipe", O_RDONLY, 662);
	return camera_process;
}

int radar_init(pid_t sensors_group, char *modalita) {
	setpgid(make_sensor("RADAR", modalita), sensors_group);
	return initialize_pipe("../tmp/radar.pipe", O_RDONLY, 662);
}

void hmi_init(struct pipe_process *hmi_processes, int processes_number) {
	(hmi_processes + READ)->pid = make_process("hmi-input", 10);
	(hmi_processes + WRITE)->pid = make_process("hmi-output", 11);
	(hmi_processes + READ)->pgid = getpgid((hmi_processes + READ)->pid);
	setpgid((hmi_processes + WRITE)->pid, (hmi_processes + READ)->pgid);
	(hmi_processes + WRITE)->pgid = getpgid((hmi_processes + READ)->pid);
	(hmi_processes + READ)->pipe_fd = initialize_pipe("../tmp/hmi-input.pipe", O_RDONLY, 0662);
	(hmi_processes + WRITE)->pipe_fd = initialize_pipe("../tmp/hmi-output.pipe", O_WRONLY, 0664);
	return;
}

struct pipe_process park_assist_init(char *modalita) {
	struct pipe_process park_process;
	park_process.pid = make_sensor("ASSIST", modalita);
	park_process.pgid = getpgid(park_process.pid);
	park_process.pipe_fd = initialize_pipe("../tmp/assist.pipe", O_RDONLY, 0662);
	return park_process;
}

void ECU_signal_handler (int sig){
	if(sig == SIGUSR1) {
		// significa che ha fallito l'accelerazione
		arrest(brake_process.pid);
		// TERMINO TUTTI I PROCESSI DOPO AVER ARRESTATO LA MACCHINA
		// NON IMPORTA UCCIDERE PARK ASSIST PERCHE' E' GIA' TERMINATO
		kill(-(processes_groups.actuators_group), SIGKILL);
		kill(-(processes_groups.sensors_group), SIGKILL);
		kill(-(processes_groups.hmi_group), SIGKILL);
	} else if(sig == SIGUSR2) {
		// significa che park_assist ha concluso la procedura di parcheggio
		kill(processes_groups.park_assist_group, SIGKILL);
		kill(sensor_signal, SIGKILL);
		// QUI ORA UCCIDO LA HMI MA PRIMA VOGLIO MANDARE UN SEGNALE DIFFERENTE
		kill(-(processes_groups.hmi_group), SIGUSR1);
		unmask(022);
		// LA SLEEP SERVE A DARE TEMPO ALLA HMI DI MOSTRARE UN MESSAGGIO PRIMA DI CHIUDERSI
		sleep(3);
		exit(EXIT_SUCCESS);
	}
}

void arrest(pid_t brake_process) {
	kill(brake_process, SIGUSR1);
	speed = 0;
}

void send_command(int actuator_pipe_fd, int log_fd, int hmi_pipe_fd, char *command, size_t command_size) {
	broad_log(actuator_pipe_fd, log_fd, command, command_size);
	write(hmi_pipe_fd, command, command_size);
}

void change_speed(int requested_speed, int throttle_pipe_fd, int brake_pipe_fd, int log_fd, int hmi_pipe_fd) {
	if(requested_speed > speed) {
		send_command(throttle_pipe_fd, log_fd, hmi_pipe_fd, "INCREMENTO 5", sizeof("INCREMENTO 5"));
		speed += 5;
	} else if(requested_speed < speed) {
		send_command(brake_pipe_fd, log_fd, hmi_pipe_fd, "FRENO 5", sizeof("FRENO 5"));
		speed -= 5;
	}
}

bool accettable_string(char *string, int length) {
	return (bool) ((strstr(string,"172A") == NULL) && (strstr(string,"D693") == NULL) && (strstr(string,"0000") == NULL) && (strstr(string,"BDD8") == NULL) && (strstr(string,"FAEE") == NULL) && (strstr(string,"4300") == NULL));
}