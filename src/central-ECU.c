#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include "../include/service-functions.h"

#define HMI_COMMAND_LENGTH 12
#define BYTES_CONVERTED 17
#define TERMINAL_NAME_MAX_LENGTH 50
#define MOD_LENGTH 12

// struttura per salvare i processi con i loro pid, il gruppo a cui appartengono e la
// pipe con cui comunicare col processo.
// brake_process e' globale perche' serve avere il pid di brake_process nell'ECU_signal_handler
struct pipe_process {
	pid_t pid;
	pid_t pgid;
	int pipe_fd;
} brake_process;

// struttura globale che serve per chiudere tutti i processi che vengono
// aperti durante l'esecuzione
struct groups {
	pid_t park_assist_group;
	pid_t actuators_group;
	pid_t sensors_group;
	pid_t hmi_group;
} processes_groups;


struct pipe_process brake_init();
struct pipe_process steer_init(pid_t);
struct pipe_process throttle_init(pid_t);
struct pipe_process camera_init();
struct pipe_process radar_init(char*);
struct pipe_process park_assist_init( char *);
void hmi_init(struct pipe_process *,int);
void arrest(pid_t);
void ECU_signal_handler(int);
void send_command(int, int, int, char *, size_t);
void change_speed(int, int, int, int, int);
bool acceptable_string(char *);

struct pipe_process *hmi_process;

int speed = 0;
int log_fd;
bool park_done_flag = false;

int main(int argc, char **argv){
	// apre un file di log per gli errori che sara' condiviso da tutti i processi figli
	umask(0000);
	// unlink("../log/errors.log");
	int error_log_fd = openat(AT_FDCWD,"log/errors.log", O_WRONLY | O_TRUNC | O_CREAT, 0666);
	dup2(error_log_fd, STDERR_FILENO);

	// controlla che gli argomenti da linea di comando siano 2 o 4 per accettare la modalita'
	// d'esecuzione e nel caso il terminale da usare per la hmi
	if(argc < 2 || argc == 3 || argc > 4 ) {
		perror("ECU: syntax error");
		exit(EXIT_FAILURE);
	}

	// controlla che il secondo argomento sia la modalita' d'esecuzione
	char *modalita = malloc(MOD_LENGTH);
	if(strcmp(argv[1],"ARTIFICIALE") && (strcmp(argv[1],"NORMALE"))) {
		perror("ECU: invalid input modality");
		exit(EXIT_FAILURE);
	} else
		modalita = argv[1];

	// FASE INIZIALIZZAZIONE SISTEMA

	// inizializza tutti i processi figli salvando dove necessario l'intero processo
	// in una struct pipe_process (nel caso di brake-by-wire, windshield-camera e park-assist)
	// mentre negli altri casi vengono salvati solo i file descriptor della pipe che serve per
	// comunicare con il nuovo processo
	brake_process = brake_init();
	struct pipe_process steer_process = steer_init(brake_process.pgid);
	struct pipe_process throttle_process = throttle_init(brake_process.pgid);
	processes_groups.actuators_group = brake_process.pgid;

	struct pipe_process camera_process = camera_init();
	pid_t sensor_signal = -camera_process.pgid;
	struct pipe_process radar_process = radar_init( modalita);
	processes_groups.sensors_group = camera_process.pgid;

	if((log_fd = openat(AT_FDCWD, "log/ECU.log", O_WRONLY | O_TRUNC | O_CREAT, 0644)) < 0)
		perror("openat ECU.log");

	// attivo il signal handler per l'errore nell'accelerazione e la conclusione
	// di parcheggio

	struct sigaction act = { 0 };
	act.sa_flags = SA_RESTART;
	act.sa_handler = &ECU_signal_handler;
	sigaction(SIGUSR1, &act, NULL);
	sigaction(SIGUSR2, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	signal(SIGCHLD, SIG_IGN);

	char *camera_buf = calloc(1, HMI_COMMAND_LENGTH);
	char *radar_buf = malloc(BYTES_CONVERTED);

	// preparo il segnale da mandare ai sensori quando dovranno essere disattivati per la
	// procedura di parcheggio
	pid_t parking_signal = -brake_process.pgid;

	// se c'e' imposta il terminale scelto dall'utente
	hmi_process = malloc(2*sizeof(struct pipe_process));
	if(argc == 4 && !(strcmp(argv[2], "--term"))) {
		if(!(hmi_process[WRITE].pid = fork()))
			execlp("./sh/new_terminal.sh", "./sh/new_terminal.sh", argv[3], NULL);
	} else {
		if(!(hmi_process[WRITE].pid = fork()))
			execlp("./sh/new_terminal.sh","./sh/new_terminal.sh" , NULL);
	}
	hmi_init(hmi_process, 2);
	processes_groups.hmi_group = (hmi_process + READ)->pgid;

	// questa variabile mi serve nel caso venga chiamato PARCHEGGIO da hmi prima che
	// venga digitato INIZIO. In questo caso diventa false e l'auto viene parcheggiata
	// immediata senza iniziare il viaggio
	bool travel_flag = true;
	bool flag_arrest = true;
	unsigned short int hmi_command;
	// ciclo d'attesa prima dell'inizio del viaggio
	while (flag_arrest){
		sleep(1);
		hmi_command = -1;
		read(hmi_process[READ].pipe_fd, &hmi_command, sizeof(unsigned short int));
		switch(hmi_command){
			case PARCHEGGIO:
				travel_flag = false;
			case INIZIO:
				flag_arrest = false;
				break;
			case ARRESTO:
				if(kill(hmi_process[READ].pid,SIGUSR2) <0)
					perror("ECU: kill");
			default: break;
		}
	}

	int requested_speed = speed;
	int temp_v;
	FILE *camera_stream;
	if((camera_stream = fdopen(camera_process.pipe_fd, "r")) == NULL){
		perror("ECU: fdopen camera");
	}

	if(kill(camera_process.pid, SIGUSR1) < 0){
		perror("ECU: kill camera");
	}

	while(travel_flag) {
		read(radar_process.pipe_fd, radar_buf, BYTES_CONVERTED);
		// legge dalla hmi esce arresta la macchina o esce dal ciclo del viaggio
		// per frenare e poi eseguire la procedura di parcheggio
		if(read(hmi_process[READ].pipe_fd, &hmi_command, sizeof(short int)) > 0){
			if(hmi_command == PARCHEGGIO)
				break;
			else if(hmi_command == ARRESTO)
				arrest(brake_process.pid);
			else
				kill(hmi_process[READ].pid, SIGUSR2);
		}
		// legge da front-windshield-camera e esegue l'azione opportuna:
		//	- esce dal ciclo per eseguire frenata e parcheggio;
		//	- arresta la macchina in caso di pericolo;
		//	- manda il comando della sterzata a steer-by-wire se deve girare;
		//	- se riceve un numero imposta la velocita' desiderata dalla camera;
		if(fgets(camera_buf, HMI_COMMAND_LENGTH, camera_stream) == NULL)
			perror("ECU: fgets camera");
		if(!strcmp(camera_buf, "PARCHEGGIO\n"))
			break;
		else if(!strcmp(camera_buf, "PERICOLO\n\0"))
			arrest(brake_process.pid);
		else if(!(strcmp(camera_buf, "DESTRA\n\0") && strcmp(camera_buf, "SINISTRA\n\0"))){
			send_command(steer_process.pipe_fd, log_fd, hmi_process[WRITE].pipe_fd, camera_buf, strlen(camera_buf)+1);
		}
		else if((temp_v = atoi(camera_buf)) > 0)
			requested_speed = temp_v;
		sleep(1);
		printf("actual speed: %d \tcommand recived: %s", speed, camera_buf);

		// aggiorna la velocita' della macchina mandando un comando INCREMENTO 5 o INCREMENTO 5
		// a throttle o brake per avvicinare di 5 la velocita' attuale a quella desiderata
		if(speed != requested_speed)
			change_speed(requested_speed, throttle_process.pipe_fd, brake_process.pipe_fd, log_fd, hmi_process[WRITE].pipe_fd);
	}

	// frena per azzerare la velocita' e cosi' poter avviare la procedura di parcheggio
	while(speed > 0) {
		change_speed(0, throttle_process.pipe_fd, brake_process.pipe_fd, log_fd, hmi_process[WRITE].pipe_fd);
		sleep(1);
	}

  // avvia la procedura di parcheggio
	if(kill(parking_signal, SIGINT) < 0)
		perror("ECU: kill parking signal");
	char park_data[BYTES_CONVERTED];
	bool parking_completed = false;
	struct pipe_process park_process;
	park_process = park_assist_init(modalita);
	processes_groups.park_assist_group = park_process.pgid;
	sleep(1);

  	while (!parking_completed) {
		broad_log(hmi_process[WRITE].pipe_fd, log_fd, "INIZIO PROCEDURA PARCHEGGIO\n\0", sizeof("INIZIO PROCEDURA PARCHEGGIO\n")+1);
		if(kill(park_process.pid, SIGUSR1) < 0)
			perror("ECU: park start signal");
		int nread = 1;
		park_done_flag = false;
    	while( !park_done_flag || nread > 0 ) {
			nread = read(park_process.pipe_fd, park_data, BYTES_CONVERTED);
    		if(!acceptable_string(park_data))
				break;
			sleep(1);
    	}

    	if(park_done_flag && nread <= 0){
			parking_completed = true;
			broad_log(hmi_process[WRITE].pipe_fd, log_fd, "PROCEDURA PARCHEGGIO COMPLETATA\n\0", sizeof("PROCEDURA PARCHEGGIO COMPLETATA\n")+1);
		} else {
			broad_log(hmi_process[WRITE].pipe_fd, log_fd, "PROCEDURA PARCHEGGIO INCOMPLETA\n", sizeof("PROCEDURA PARCHEGGIO INCOMPLETA\n")+1);
    		kill(park_process.pid, SIGUSR2);
		}
	}
	sleep(1);
  	broad_log(hmi_process[WRITE].pipe_fd, log_fd, "TERMINAZIONE PROGRAMMA\n\0", sizeof("TERMINAZIONE PROGRAMMA\n")+1);

  // termino i processi dei sensori, degli attuatori e di park-assist
	printf("pippopippo\n");
  	kill(park_process.pid, SIGINT);
	kill(hmi_process[READ].pid, SIGKILL);

  	return 0;
}

struct pipe_process brake_init() {
	struct pipe_process brake_process;
	brake_process.pid = make_process("brake-by-wire", 14, 0, NULL);
	brake_process.pipe_fd = initialize_pipe("tmp/brake.pipe", O_WRONLY, 0666);
	brake_process.pgid = getpgid(brake_process.pid);
	return brake_process;
}

struct pipe_process steer_init(pid_t actuator_group) {
	struct pipe_process steer_process;
	steer_process.pid = make_process("steer-by-wire",14,brake_process.pgid, NULL);
	setpgid(steer_process.pid, actuator_group);
	steer_process.pgid = actuator_group; 
	steer_process.pipe_fd = initialize_pipe("tmp/steer.pipe", O_WRONLY, 0666);
	return steer_process;
}

struct pipe_process throttle_init(pid_t actuator_group) {
	struct pipe_process throttle_process;
	throttle_process.pid = make_process("throttle-control", 17,brake_process.pgid, NULL);
	throttle_process.pgid = setpgid(throttle_process.pid, actuator_group);
	throttle_process.pipe_fd = initialize_pipe("tmp/throttle.pipe", O_WRONLY, 0666);
	return throttle_process;
}

struct pipe_process camera_init() {
	struct pipe_process camera_process;
	camera_process.pid = make_process("windshield-camera", 18,brake_process.pgid, NULL);
	camera_process.pipe_fd = initialize_pipe("tmp/camera.pipe", O_RDONLY, 0666);
	camera_process.pgid = getpgid(brake_process.pid);
	setpgid(camera_process.pid, brake_process.pgid);
	return camera_process;
}

struct pipe_process radar_init(char *modalita) {
	struct pipe_process radar_process;
	radar_process.pid = make_sensor("RADAR", modalita);
	radar_process.pipe_fd = initialize_pipe("tmp/radar.pipe", O_RDONLY, 0666);
	return radar_process;
}

void hmi_init(struct pipe_process *hmi_processes, int processes_number) {
	(hmi_processes + READ)->pid = make_process("hmi-input", 10, getpid(), NULL);
	(hmi_processes + READ)->pipe_fd  = initialize_pipe("tmp/hmi-in.pipe", O_RDONLY | O_NONBLOCK, 0666);
	(hmi_processes + WRITE)->pipe_fd = initialize_pipe("tmp/hmi-out.pipe", O_WRONLY, 0666);
	(hmi_processes + READ)->pgid = getpgid((hmi_processes + READ)->pid);
	(hmi_processes + WRITE)->pgid = getpgid((hmi_processes + READ)->pid);
	return;
}

struct pipe_process park_assist_init(char *modalita) {
	struct pipe_process park_process;
	park_process.pid = make_process("park-assist", 12, 0, modalita);
	park_process.pipe_fd = initialize_pipe("tmp/assist.pipe", O_RDONLY | O_NONBLOCK, 0666);
	park_process.pgid = getpgid(park_process.pid);
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
		kill(-(processes_groups.hmi_group), SIGUSR1);
	} else if( sig == SIGUSR2)
			park_done_flag = true; 
	 else if(sig == SIGINT){
			// significa che park_assist ha concluso la procedura di parcheggio
			if(kill(-processes_groups.actuators_group, SIGKILL) < 0)
				perror("ECU: kill actuators");
			kill(-processes_groups.sensors_group, SIGKILL);
			kill(-processes_groups.park_assist_group, SIGKILL);
			kill(-processes_groups.hmi_group, SIGKILL);
			umask(022);
			exit(EXIT_SUCCESS);
	}
}

void arrest(pid_t brake_process) {
	broad_log(hmi_process[WRITE].pipe_fd, log_fd, "ARRESTO AUTO\n\0", sizeof("ARRESTO AUTO\n")+1);
	kill(brake_process, SIGUSR1);
	speed = 0;
}

void send_command(int actuator_pipe_fd, int log_fd, int hmi_out_fd, char *command, size_t command_size) {
	broad_log(actuator_pipe_fd, log_fd, command, command_size);
	write(hmi_out_fd, command, command_size);
}

void change_speed(int requested_speed, int throttle_pipe_fd, int brake_pipe_fd, int log_fd, int hmi_pipe_fd) {
	if(requested_speed > speed) {
		send_command(throttle_pipe_fd, log_fd, hmi_pipe_fd, "INCREMENTO 5\n", sizeof("INCREMENTO 5\n"));
		speed += 5;
	} else if(requested_speed < speed) {
		send_command(brake_pipe_fd, log_fd, hmi_pipe_fd, "FRENO 5\n", sizeof("FRENO 5\n"));
		speed -= 5;
	}
}

bool acceptable_string(char *string) {
	return (bool) ((strstr(string,"5152") == NULL) && (strstr(string,"D693") == NULL) && (strstr(string,"0000") == NULL) && (strstr(string,"BDD8") == NULL) && (strstr(string,"FAEE") == NULL) && (strstr(string,"4300") == NULL));
}