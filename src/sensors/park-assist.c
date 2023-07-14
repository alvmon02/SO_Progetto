#include <linux/limits.h>
#define _POSIX_C_SOURCE 200809L

/*Inclusione delle librerie necessarie*/
#include "../../include/service-functions.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define ECU_MESSAGE_MAX_LEN 15

void signal_handler(int);
int log_open();
int initialize_client_socket(const char *);
FILE *pipe_open();

bool start_flag;
bool restart_flag;

int main(int argc, char *argv[]) {

  signal(SIGINT, &signal_handler);
  // struct sigaction act = { 0 };
  // act.sa_flags = SA_RESTART;
  // act.sa_handler = &signal_handler;
  // sigaction(SIGINT, &act, NULL);

  if (argc != 2) {
    perror("park-assist: syntax error");
    exit(EXIT_FAILURE);
  }

  pid_t cameras_pid;
  int log_fd, cameras_fd, counter, sock_fd;
  // Apertura del log file
  log_fd = log_open();

  char *hex_translation = malloc(BYTES_CONVERTED), *mode = argv[1],
       *buf = calloc(1, ECU_MESSAGE_MAX_LEN);

  int input_fd;
  if (!strcmp(mode, "ARTIFICIALE")) {
    input_fd = openat(AT_FDCWD, "urandomARTIFICIALE.binary", O_RDONLY);
    perror("park: open ARTIFICIALE");
  } else {
    input_fd = open("/dev/urandom", O_RDONLY);
    perror("park: open NORMALE");
  }

  // Creazione di un processo figlio per la gestione del surround-view-cameras
  cameras_pid = make_sensor(CAMERAS, mode, getpid());
  // Creazione della pipe per leggere la informazione
  // arrivata da bytes-sensor
  cameras_fd = initialize_pipe("./tmp/cameras.pipe", O_RDONLY, 0666);

  // Connessione alla ECU
  sock_fd = initialize_client_socket("./tmp/assist.sock");
  perror("park: socket connected");

  do {
    read(sock_fd, buf, ECU_MESSAGE_MAX_LEN);
    printf("%s\n", buf);
    sleep(1);
  } while (strcmp(buf, "INIZIO\n"));

  // La Scrittura della informazione sulla pipe di comunicazione con la ECU
  // fatta nella funzione broad_log, anche con la scrittura della info
  // sullo file assist.log.
  // La Lettura della informazione dalla pipe
  // già fatta nella parte iniziale di read_conv_broad,
  // dato che cominzia con la lettura da park_assist,
  // e dopo si traduce il messagio, per il invio
  // a la ECU.
  while (true) {
    counter = 0;

    while (counter++ < PARK_TIME) {
      printf("Scrittura park numero: %d\n", counter);
      if (read_conv_broad(input_fd, hex_translation, sock_fd, log_fd) <= 0) {
        lseek(input_fd, 0, SEEK_SET);
        perror("park: input file terminated: file pointer reset");
      }
      read(sock_fd, buf, ECU_MESSAGE_MAX_LEN);
      if (!strcmp(buf, "RIAVVIO\n"))
        break;
      if (read(cameras_fd, hex_translation, BYTES_CONVERTED) < 0)
        perror("park: unable to read cameras pipe");
      else{
      	printf("Scrittura cameras numero: %d\n", counter);
        write(sock_fd, hex_translation, BYTES_CONVERTED);
	  }
      read(sock_fd, buf, ECU_MESSAGE_MAX_LEN);
      if (!strcmp(buf, "RIAVVIO\n")) {
        break;
      }
    }
    if (!strcmp(buf, "RIAVVIO\n")) {
      perror("park: incomplete park: restarting cycle");
      continue;
    }
    write(sock_fd, "FINITO\n\0", sizeof("FINITO\n") +1);
    kill(cameras_pid, SIGKILL);
	close(sock_fd);
    exit(EXIT_SUCCESS);
  }
  // }
  // if(restart_flag){
  // 	perror("park: incomplete park: restarting cycle");
  // 	continue;
  // }
  // kill(cameras_pid, SIGTSTP);
  // sleep(2);
  // printf("Pippo\n");
  // if(kill(getppid(), SIGUSR2) < 0)
  // 	perror("park: signaling ecu finished to write");
  // while(!restart_flag)
  // 	sleep(1);
  // kill(cameras_pid, SIGCONT);
}

int log_open() {
  int fd;
  // Apertura del logfile
  fd = openat(AT_FDCWD, "log/assist.log", O_WRONLY | O_TRUNC | O_CREAT, 0666);
  if (fd < 0) {
    perror("park assist: openat log");
    exit(EXIT_FAILURE);
  }
  return fd;
}

int initialize_client_socket(const char *socket_name) {
  int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, socket_name);
  while (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    sleep(1);
  return sock_fd;
}

void signal_handler(int sig) {
  if (sig == SIGINT) {
    kill(0, SIGKILL);
    exit(EXIT_SUCCESS);
  }
}
