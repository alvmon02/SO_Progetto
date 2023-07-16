#ifndef SERVICE_FUNCTIONS_H_DEFINED
#define SERVICE_FUNCTIONS_H_DEFINED

	#include <sys/types.h>

	#define IN 0
	#define OUT 1
	#define LEFT 0
	#define RIGHT 1
	#define THROTTLE 0
	#define BRAKE 1
	#define INIZIO 0
	#define ARRESTO 1
	#define PARCHEGGIO 2
	#define RADAR "RADAR"
	#define CAMERAS "CAMERAS"
	#define ASSIST "ASSIST"
	#define SURR_CAM_LEN 8
	#define PARK_TIME 30
	#define CONTINUE 0
	#define RELOAD 1
	#define BYTES_LEN 8
	#define BYTES_CONVERTED 17

	int initialize_pipe(char * pipe_pathname, int flags, mode_t mode);

	void hex(unsigned char* to_conv, size_t size_to_conv,  char* converted);

	void broad_log (int pipe_fd, int log_fd, char * message, size_t size);

	int read_conv_broad(int input_fd, char * input_hex, int comm_fd, int log_fd);

	char * str_toupper(char * str);

	void time_log_func ( int log_fd, size_t size, short int proc);

	pid_t make_process(char *program_name, int name_length, pid_t pgid, char *args);

	pid_t make_sensor(char *, char *, pid_t);

#endif // SERVICE_FUNCTIONS_H_DEFINED
