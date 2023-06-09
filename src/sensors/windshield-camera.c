#include <error.h>     /*per funzione perror*/
#include <fcntl.h>     /*per open*/
#include <signal.h>    /*per la funzione kill*/
#include <stdio.h>     /*per sprintf*/
#include <stdlib.h>    /*per funzione exit*/
#include <sys/stat.h>  /*per mknod (sys call per mkfifo)*/
#include <sys/types.h> /*per mkfifo*/
#include <time.h> /*per le funzioni e le strutture relative a time, vedi
logging( ) */
#include <unistd.h> /*per sistem calls e pipe*/


#define INPUT_MAX_LEN 11

short int log_fd;
short int pipe_fd;

void broadcast_input(char [], int);
void log_func(char *, int);

int main() { // COMMENTI DA CAMBIARE TUTTI!

  unlink("../tmp/camera.pipe");
  mkfifo("../tmp/camera.pipe", 0600);

  pipe_fd = open("../tmp/camera.pipe", O_WRONLY);
  log_fd = open("../log/camera.log", O_WRONLY | O_APPEND | O_CREAT, 0644);

	FILE *input_file = fopen("../include/frontCamera.data", "r");

  char *camera_input = malloc(INPUT_MAX_LEN);
  size_t input_len;
  int nread;

  while (1) {
    if ((nread = getline(&camera_input, &input_len, input_file)) > 0) {
/*stringa
massima: "PARCHEGGIO\0"*/
      broadcast_input(camera_input, input_len);
      log_func(camera_input, input_len);
    }
    sleep(1);
  }
}

void broadcast_input(char message[], int size) {
  write(pipe_fd, &message, size); /*i caratteri contati nel caso della funzione
getline sono comprensivi del \n o del EOF, quindi size comprende anche questi
caratteri*/
  return;
}

void log_func(char *log_phrase, int size) {
  write(log_fd, log_phrase, size);
  return;
}
