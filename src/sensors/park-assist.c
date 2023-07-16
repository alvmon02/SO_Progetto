#include <linux/limits.h>
#define _POSIX_C_SOURCE 200809L

/*Inclusione delle librerie necessarie*/
#include "../../include/service-functions.h"
#include <ctype.h>
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

int log_open();
int initialize_client_socket(const char *);

int main(int argc, char *argv[]) {

  // Controllo che park assist venga chiamato con la modalita' come argomento
  if (argc != 2) {
    perror("park-assist: syntax error");
    exit(EXIT_FAILURE);
  }

  /* Dichiarazione del pid di surround-view-cameras, dei file descriptors del file di log assist.log, della pipe per
   * comunicare con surround-view-cameras e della socket per comunicare con ECU e del contatore necessario per
   * contare i secondi della manovra di parcheggio
   */
  pid_t cameras_pid;
  int log_fd, cameras_fd, sock_fd, counter;
  // Apertura del log file
  log_fd = log_open();

  // Inizializzazione dei buffer dove vengono salvati rispettivamente i dati letti dal file di input e dalla socket connessa alla ECU  
  char *hex_translation = malloc(BYTES_CONVERTED), *buf = calloc(1, ECU_MESSAGE_MAX_LEN);

  // Apertura del file da cui prendere i dati in base al valore della modalita' ricevuto come argomento
  int input_fd;
  if (!strcmp(argv[1], "ARTIFICIALE")) {
    if((input_fd = openat(AT_FDCWD, "urandomARTIFICIALE.binary", O_RDONLY)) < 0)
      perror("park: open ARTIFICIALE");
  } else {
    if((input_fd = open("/dev/urandom", O_RDONLY)) < 0)
      perror("park: open NORMALE");
  }

  // Creazione di un processo figlio per la gestione del surround-view-cameras
  cameras_pid = make_sensor(CAMERAS, argv[1], getpid());

  // Creazione della pipe per leggere l'informazione
  // arrivata da bytes-sensor
  cameras_fd = initialize_pipe("./tmp/cameras.pipe", O_RDONLY | O_NONBLOCK, 0666);

  // Creazione e inizializzazione della socket usata per comunicare con la ECU
  sock_fd = initialize_client_socket("./tmp/assist.sock");

  // Ciclo d'attesa: il parcheggio inizia quando viene letta dalla socket la stringa "INIZIO\n"
  do {
    read(sock_fd, buf, ECU_MESSAGE_MAX_LEN);
    sleep(1);
  } while (strcmp(buf, "INIZIO\n"));

  /*
    CICLO DI PARCHEGGIO
    Qui inizia la manovra di parcheggio vero e proprio.
    Il ciclo piu' esterno se non viene interrotto rappresenta trenta secondi di manovre necessarie per un
    parcheggio completo, quello piu' interno invece e' costituito dalle letture dal file di input e dalla
    pipe connessa a surround-view-camera e dalla scrittura dei due dati nella socket per mandarli alla ECU.
    Inoltre due volte ogni secondo park-assist legge dalla socket i messaggi della ECU che possono causare  
  */
  while (true) {
    counter = 0;

    while (counter++ < PARK_TIME) {
      // Lettura da file di park-assist e invio alla socket della ECU
      if (read_conv_broad(input_fd, hex_translation, sock_fd, log_fd) <= 0) {
        lseek(input_fd, 0, SEEK_SET);
        perror("park: input file terminated: file pointer reset");
      }
      // Lettura del messaggio della ECU dalla socket
      read(sock_fd, buf, ECU_MESSAGE_MAX_LEN);
      // Valutazione del possibile riavvio della manovra di parcheggio
      if (!strcmp(buf, "RIAVVIO\n"))
        break;
      // Lettura dei dati inviati da surround-view-cameras
      if (read(cameras_fd, hex_translation, BYTES_CONVERTED) < 0)
        perror("park: unable to read cameras pipe");
      else{
        // Invio dei dati di surround-view-cameras alla ECU
        write(sock_fd, hex_translation, BYTES_CONVERTED);
	    }
      // Lettura per un eventuale ordine di riavvio della manovra di parcheggio dalla ECU
      read(sock_fd, buf, ECU_MESSAGE_MAX_LEN);
      if (!strcmp(buf, "RIAVVIO\n")) {
        break;
      }
    }
    // Se il programma e' uscito dal ciclo per un ordine di riavvio questo if permette l'esecuzione di un
    // altro ciclo di manovre di parcheggio
    if (!strcmp(buf, "RIAVVIO\n")) {
      perror("park: incomplete park: restarting cycle");
      continue;
    }
    // Prima di chiudersi park-assist comunica che si e' conclusa la manovra di parcheggio alla ECU che 
    // provvedera' a terminare l'intero programma
    write(sock_fd, "FINITO\n\0", sizeof("FINITO\n") +1);
    kill(cameras_pid, SIGKILL);
	  close(sock_fd);
    exit(EXIT_SUCCESS);
  }
}

// Funzione che apre il file assist.log
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

// Funzione che inizializza il lato client della socket usata da park-assist per comunicare con ECU
int initialize_client_socket(const char *socket_name) {
  int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, socket_name);
  while (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    sleep(1);
  return sock_fd;
}
