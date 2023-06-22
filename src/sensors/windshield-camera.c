#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include "../../include/service-functions.h"

// MACROS
// INPUT_MAX_LEN: lunghezza massima della stringa proveniente dal file di
// input: "PARCHEGGIO\0"
#define INPUT_MAX_LEN 11

// File descriptor del log file
int log_fd;
// File descriptor del pipe in scrittura
int pipe_fd;

//La funzione main esegue le operazioni relative al componente windshield-camera
int main ( ) {

  // Connessione del file descriptor del pipe per la comunicazione tra central
  // ECU e windshield-camera. Il protocollo impone che il pipe sia creato
  // durante la fase di inizializzazione del processo central-ECU e che venga
  // aperto in sola scrittura dal processo windshield-camera il quale vi scriva
  // una volta al secondo.
  if((pipe_fd = open("../tmp/camera.pipe", O_WRONLY))){
    perror("open pipe");
    exit(EXIT_FAILURE);
  }
  // Connessione del file descriptor del log file. Apertura in sola scrittura.
  // Qualora il file non esista viene creato. Qualora il file sia presente
  // si mantengono le precedenti scritture. Data l'esecuzione dell'unlink
  // da parte della central-ECU, non vi saranno scritture pendenti da
  // precedenti esecuzioni.
  if(unlink("../../log/camera.log") < 0){
    perror("unlink log");
    exit(EXIT_FAILURE);
  }
  if((log_fd = open("../../log/camera.log", O_WRONLY | O_APPEND | O_CREAT, 0644)) < 0){
    perror("open log");
    exit(EXIT_FAILURE);
  }
  // Inizializzazione e connessione del file descriptor al file da cui
  // ottenere i dati di input. Apertura in sola lettura all'inizio del file.
  int input_fd;
  if((input_fd = open("../include/frontCamera.data", O_RDONLY)) < 0){
    perror("open input");
    exit(EXIT_FAILURE);
  }

  // Inizializzazione della stringa di input che rappresenta il messaggio da
  // trasmettere alla central-ECU da parte del processo.
  char *camera_input = malloc(INPUT_MAX_LEN);
  if(camera_input == NULL){
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  int nread;

  // Il ciclo successivo rappresenta il cuore del processo.
  // Ad ogni lettura del file di input accadrà che il file non ha
  // raggiunto ancora la terminazione, ad eccezione dell'ultima lettura
  // nella quale verrà ottenuta una stringa EOF, quindi nread = 0.
  // Fintantoché il file non termina i dati vengono trasmessi alla
  // central-ECU, una volta terminato il processo termina con codice
  // EOF (=0).
  while (true) {
    if((nread = read(input_fd, camera_input, INPUT_MAX_LEN)) < 0){
      perror("read");
      exit(EXIT_FAILURE);
    } else if (nread){
      broad_log(pipe_fd, log_fd, camera_input, INPUT_MAX_LEN);
      sleep(1);
    } else
      exit(EXIT_SUCCESS);
  }
}
