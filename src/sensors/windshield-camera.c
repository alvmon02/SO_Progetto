#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "../../include/service-functions.h"

// MACROS
// INPUT_MAX_LEN: lunghezza massima della stringa proveniente dal file di
// input: "PARCHEGGIO\0"
#define INPUT_MAX_LEN 11

void start_handler ( int );

// File descriptor del log file
int log_fd;
// File descriptor del pipe in scrittura
int pipe_fd;

int input_fd;

bool start_flag = false;

//La funzione main esegue le operazioni relative al componente windshield-camera
int main ( ) {
  errno = 0;
  signal(SIGUSR1, start_handler);
  perror("windshield: signal eseguita");
  // Connessione del file descriptor del pipe per la comunicazione tra central
  // ECU e windshield-camera. Il protocollo impone che il pipe sia creato
  // durante la fase di inizializzazione del processo central-ECU e che venga
  // aperto in sola scrittura dal processo windshield-camera il quale vi scriva
  // una volta al secondo.
  errno = 0;
  while((pipe_fd = openat(AT_FDCWD, "tmp/camera.pipe", O_WRONLY)) < 0){
    perror("windshield: openat pipe");
    sleep(1);
  }
  perror("windshield: CONNECTED");
  // Connessione del file descriptor del log file. Apertura in sola scrittura.
  // Qualora il file non esista viene creato. Qualora il file sia presente
  // si mantengono le precedenti scritture. Data l'esecuzione dell'unlink
  // da parte della central-ECU, non vi saranno scritture pendenti da
  // precedenti esecuzioni.
  if((log_fd = openat(AT_FDCWD, "log/camera.log", O_WRONLY | O_TRUNC | O_CREAT, 0644)) < 0){
    perror("windshield: openat log");
    exit(EXIT_FAILURE);
  }
  // Inizializzazione e connessione del file descriptor al file da cui
  // ottenere i dati di input. Apertura in sola lettura all'inizio del file.
  if((input_fd = openat(AT_FDCWD, "frontCamera.data", O_RDONLY)) < 0){
    perror("windshield: open input");
    exit(EXIT_FAILURE);
  }

  // Inizializzazione della stringa di input che rappresenta il messaggio da
  // trasmettere alla central-ECU da parte del processo.
  char *camera_input = malloc(INPUT_MAX_LEN);
  if(camera_input == NULL){
    perror("windshield: malloc");
    exit(EXIT_FAILURE);
  }

  while(!start_flag){
    perror("windshield: start_flag is false");
    sleep(1);
  }

  // Il ciclo successivo rappresenta il cuore del processo.
  // Ad ogni lettura del file di input accadrà che il file non ha
  // raggiunto ancora la terminazione, ad eccezione dell'ultima lettura
  // nella quale verrà ottenuta una stringa EOF, quindi nread = 0.
  // Fintantoché il file non termina i dati vengono trasmessi alla
  // central-ECU, una volta terminato il processo termina con codice
  // EOF (=0).
  FILE * input_file = fdopen(input_fd, "r");
  while (true) {
   if(fgets(camera_input, INPUT_MAX_LEN, input_file) == NULL){
      perror("windshield: fgets");
      exit(EXIT_SUCCESS);
    } else {
      broad_log(pipe_fd, log_fd, camera_input, strlen(camera_input));
      sleep(1);
    }
  }
}

void start_handler(int sig){
  start_flag = true;
  errno = 0;
  perror("windshield: handler: start_flag is true");
}
