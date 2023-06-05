#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

int steer_init ( );
void turn ( int , char * );
void throttle_failure_handler ( );
void arrest( );

int main(){
	signal(SIGCHLD, throttle_failure_handler);
	int steer_pipe_fd = steer_init();
}


int steer_init ( ) {
	if(fork())
		execl("./steer-by-wire", "steer-by-wire", NULL);
	else{
		int pipe_fd;
		while(pipe_fd = open ("../log/steer.log", O_WRONLY) >= 0)
			sleep(1);
		return pipe_fd;
	}
}

void turn (int steer_pipe_fd, char *direction ){
	write (steer_pipe_fd, direction, 9);
	return;
}

void throttle_failure_handler ( ){
	// arrest();
}

