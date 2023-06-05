
#ifndef SERVICE_FUNCTIONS_H_DEFINED
	#define SERVICE_FUNCTIONS_H_DEFINED
	#define READ 0
	#define WRITE 1
	short int initialize_socket(char * sock_pathname, int domain, int type, int
queue_len);
	short int initialize_pipe(char * pipe_pathname, int flags, mode_t mode);

#endif // SERVICE_FUNCTIONS_H_DEFINED
