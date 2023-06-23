#ifndef SERVICE_FUNCTIONS_H_DEFINED
#define SERVICE_FUNCTIONS_H_DEFINED

	#include <sys/types.h>

	#define READ 0
	#define WRITE 1
	#define LEFT 0
	#define RIGHT 1
	#define THROTTLE 0
	#define BRAKE 1
	#define INIZIO 0
	#define ARRESTO 1
	#define PARCHEGGIO 2
	#define NORMALE "NORMALE"
	#define ARTIFICIALE "ARTIFICIALE"
	#define RADAR "RADAR"
	#define CAMERAS "CAMERAS"
	#define ASSIST "ASSIST"
	#define BYTES_LEN 8
	#define SURR_CAM_LEN 8
	#define PARK_TIME 30

	int initialize_pipe(char * pipe_pathname, int flags, mode_t mode);

	void hex(unsigned char* to_conv, size_t size,  char* converted);

	void broad_log (int pipe_fd, int log_fd, char * message, size_t size);

	void read_conv_broad(int input_fd, unsigned char * input_str, char * input_hex, int comm_fd, int log_fd);

	char * str_toupper(char * str);

	void time_log_func ( int log_fd, size_t size, short int proc);

	pid_t make_process(char *program_name, int name_length, char *args);

#endif // SERVICE_FUNCTIONS_H_DEFINED
