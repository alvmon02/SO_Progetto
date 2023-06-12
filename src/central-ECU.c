#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

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
}



int steer_init ( ) {
	if(!fork())
		execl("./steer-by-wire", "steer-by-wire", NULL);
	else{
		int pipe_fd;
		while(pipe_fd = open ("../temp/steer.pipe", O_WRONLY) >= 0)
			sleep(1);
		return pipe_fd;
	}
}

int throttle_init ( ) {
	if(!fork())
		execl("./throttle-control", "throttle-control", NULL);
	else
		return initialize_pipe("../temp/throttle.pipe", O_WRONLY, 660);
}

// questo pero' deve restituire anche il pid del processo creato per segnale d'arresto
int brake_init (pid_t *brake_process_addr) {
	if((*brake_process_addr = fork()) == 0)
		execl("./brake-by-wire", "brake-by-wire", NULL);
	else
		return initialize_pipe("../temp/brake.pipe", O_WRONLY, 660);
}

int camera_init () {
	if(!fork())
		execl("./windshield-camera", "windshield-camera", NULL);
	else
		return initialize_pipe("../temp/camera.pipe", O_RDONLY, 660);
}

int radar_init () {
	if(!fork())
		execl("./forward-facing-radar", "forward-facing-radar", NULL);
	else
		return initialize_pipe("../temp/radar.pipe", O_RDONLY, 660);
}

int hmi_init () {
	initialize_socket("../tmp/hmi.sock", AF_UNIX, SOCK_STREAM, 5);
}

int park_assist_init() {
	if(!fork())
		execl("./park-assist", "park-asssist", NULL);
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

