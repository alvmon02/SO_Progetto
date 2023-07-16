#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include "../../include/service-functions.h"

// MACROS
// INPUT_MAX_LEN: lunghezza massima della stringa proveniente dal file di
// input: "PARCHEGGIO\n"
#define INPUT_MAX_LEN 12

void start_handler ( int );
bool start_flag = false;

//La funzione main esegue le operazioni relative al componente windshield-camera
int main ( ) {
  // File descriptor del log file
  int log_fd;
  // File descriptor del pipe in scrittura
  int pipe_fd;
  // File descriptor del file da cui prende i dati in input 
  int input_fd;

  // Al segnale SIGUSR1 viene associato il gestore start_handler che avvia la lettura dei dati da file
  // quando la ECU manda il segnale che indica che e' iniziato il viaggio della automobile 
  signal(SIGUSR1, start_handler);

  // Connessione del file descriptor del pipe per la comunicazione tra central
  // ECU e windshield-camera. Il protocollo impone che il pipe sia creato
  // durante la fase di inizializzazione del processo central-ECU e che venga
  // aperto in sola scrittura dal processo windshield-camera il quale vi scriva
  // una volta al secondo.
  while((pipe_fd = openat(AT_FDCWD, "tmp/camera.pipe", O_WRONLY)) < 0){
    perror("windshield: openat pipe");
    sleep(1);
  }
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
  char *camera_input = calloc(1, INPUT_MAX_LEN);
  if(camera_input == NULL){
    perror("windshield: malloc");
    exit(EXIT_FAILURE);
  }

  while(!start_flag){
    sleep(1);
  }

  // Il ciclo successivo rappresenta il cuore del processo.
  // Ad ogni lettura il processo potra' o leggere dei dati o, con nread = 0, essere arrivato alla
  // fine del file.
  // Fintantoché il file non termina i dati vengono trasmessi alla
  // central-ECU, una volta terminato il processo termina.
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

// Imposta il flag start_flag true cosi' che il processo esca dal ciclo d'attesa ed entri in quello di lettura
void start_handler(int sig){
  start_flag = true;
}
