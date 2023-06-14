
#ifndef SERVICE_FUNCTIONS_H_DEFINED
	#define SERVICE_FUNCTIONS_H_DEFINED

	#include <sys/types.h>
	#define READ 0
	#define WRITE 1
	#define NORMALE 0
	#define ARTIFICIALE 1
	#define RADAR 0
	#define CAMERAS 1
	#define ASSIST 2

	short int initialize_socket(char * sock_pathname, int domain, int type, int
queue_len);

	short int initialize_pipe(char * pipe_pathname, int flags, mode_t mode);

	char * hex (unsigned long bytes);

	void bytes_log (int log_fd, unsigned long bytes, size_t size);

	void broad_log (int pipe_fd, int log_fd, char * message, size_t size);

	char * str_toupper(char * str);

#endif // SERVICE_FUNCTIONS_H_DEFINED
