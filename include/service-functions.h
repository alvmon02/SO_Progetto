
#ifndef SERVICE_FUNCTIONS_H_DEFINED
	#define SERVICE_FUNCTIONS_H_DEFINED

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

	void broadcast_input (int pipe_fd, char * message, size_t size);

	void log_func (int log_fd, char * log_phrase, size_t size);

	void broad_log (int pipe_fd, int log_fd, char * message, size_t size);

	char * str_toupper(char * str);

#endif // SERVICE_FUNCTIONS_H_DEFINED
