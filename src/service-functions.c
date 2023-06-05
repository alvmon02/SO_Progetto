#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <service-functions.h>

short int initialize_socket(char * sock_pathname, int domain, int type, int
queue_len){
	short int fd;
	unlink(sock_pathname);
	fd = socket(domain, type, 0);
	struct sockaddr_un addr;
	addr.sun_family = domain;
	strcpy(addr.sun_path, sock_pathname);
	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr))) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	if (listen (fd, queue_len)){
		perror("listen");
		exit(EXIT_FAILURE);
	}
	return fd;
}

short int initialize_pipe(char * pipe_pathname, int flags, mode_t mode){
	short int fd;
	unlink (pipe_pathname);
	mkfifo(pipe_pathname, mode);
	return (fd = open(pipe_pathname, flags));
}
