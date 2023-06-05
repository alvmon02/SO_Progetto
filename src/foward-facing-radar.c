#include <error.h>     /*per funzione perror*/
#include <fcntl.h>     /*per open*/
#include <signal.h>    /*per la funzione kill*/
#include <stdio.h>     /*per sprintf, getline */
#include <stdlib.h>    /*per funzione exit*/
#include <sys/stat.h>  /*per mknod (sys call per mkfifo)*/
#include <sys/types.h> /*per mkfifo*/
#include <time.h> /*per le funzioni e le strutture relative a time, vedi logging ( ) */
#include <unistd.h> /*per sistem calls e pipe*/

#define INPUT_LEN 8

short int log_fd;
short int pipe_fd;

void broadcast_input(char *, int);
void log_func(char *, int);

int main() { // COMMENTI DA CAMBIARE TUTTI!

  unlink("../tmp/radar.pipe");
  mkfifo("../tmp/radar.pipe", 0600);

  pipe_fd = open("../tmp/radar.pipe", O_WRONLY);
  log_fd = open("../log/radar.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
  short int input_fd = open("../dev/urandom", O_RDONLY, 0400);

  char radar_input[INPUT_LEN];

  while (1) {

    if ((read(input_fd, radar_input, INPUT_LEN)) == INPUT_LEN) {
      broadcast_input(radar_input, INPUT_LEN);
      log_func(radar_input, INPUT_LEN);
    }
    sleep(1);
  }
}

void broadcast_input(char *message, int size) {
  write(pipe_fd, &message, INPUT_LEN);
  return;
}

void log_func(char *log_phrase, int size) {
  write(log_fd, log_phrase, INPUT_LEN);
  return;
}
