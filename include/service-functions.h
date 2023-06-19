#ifndef SERVICE_FUNCTIONS_H_DEFINED
#define SERVICE_FUNCTIONS_H_DEFINED

	#include <sys/types.h>
	#define READ 0
	#define WRITE 1
	#define NORMALE "NORMALE"
	#define ARTIFICIALE "ARTIFICIALE"
	#define RADAR "RADAR"
	#define CAMERAS "CAMERAS"
	#define ASSIST "ASSIST"
	#define SURR_CAM_LEN 8

	short int initialize_socket(char * sock_pathname, int domain, int type, int queue_len);

	short int initialize_pipe(char * pipe_pathname, int flags, mode_t mode);

	void hex(unsigned char* to_conv, size_t size,  char* converted);

	void bytes_log (int log_fd, unsigned long bytes, size_t size);

	void read_output (int pipe_fd, char * message_out, size_t size);

	void log_func (int log_fd, char * log_phrase, size_t size);

	void broad_log (int pipe_fd, int log_fd, char * message, size_t size);

	char * str_toupper(char * str);

#endif // SERVICE_FUNCTIONS_H_DEFINED
