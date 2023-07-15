#define _POSIX_C_SOURCE 200809L

#include "../include/service-functions.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define HMI_COMMAND_LENGTH 12
#define BYTES_CONVERTED 17
#define TERMINAL_NAME_MAX_LENGTH 50
#define MOD_LENGTH 12

/*
  struttura per salvare i processi con i loro pid, il gruppo a cui appartengono
  e il file descriptor della socket o pipeche serve per comunicare col processo
  brake_process e park_process sono globali perche' e' necessario conoscere
  i loro pid e pgid nell'ECU_signal_handler
*/
struct process {
  pid_t pid;
  pid_t pgid;
  int comm_fd;
} brake_process, park_process;

/*
  struttura globale che serve per raggruppare i processi nelle differenti
  categorie per poterli chiudere con un solo comando in maniera piu' leggibile
*/
struct groups {
  pid_t park_assist_group;
  pid_t actuators_group;
  pid_t sensors_group;
  pid_t hmi_group;
} processes_groups;

struct process brake_init();
struct process steer_init();
struct process throttle_init(pid_t);
struct process camera_init();
struct process radar_init(char *);
struct process park_assist_init(char *);
int initialize_server_socket(const char *);
void hmi_init(struct process *, int);
void arrest(pid_t);
void ECU_signal_handler(int);
void send_command(int, int, int, char *, size_t);
void change_speed(int, int, int, int, int);
bool acceptable_string(char *);

/*
  Anche le seguenti variabili sono globali perche' devono essere accessibili ai differenti
  signal hadlers direttamente o indirettamente
*/
struct process *hmi_process;

// Velocita' del veicolo, definita a livello globale per permettere l'arresto 
// in caso di throttle failure
int speed = 0;
// File descriptor del file di log della central ECU
int log_fd;

int main(int argc, char **argv) {
  // apre un file di log per gli errori che sara' condiviso da tutti i processi figli
  umask(0000);
  int error_log_fd =
      openat(AT_FDCWD, "log/errors.log", O_WRONLY | O_TRUNC | O_CREAT, 0666);
  dup2(error_log_fd, STDERR_FILENO);

  /*
    Qui avviene il controllo delle opzioni da linea di comando.
    Si possono avere solo o due o quattro argomenti:
    due se si sceglie solo la modalita' d'esecuzione col terminale di default,
    quattro se si sceglie anche quale terminale usare
  */
  if (argc < 2 || argc == 3 || argc > 4) {
    perror("ECU: syntax error");
    exit(EXIT_FAILURE);
  }

  // Controlla che il secondo argomento sia effettivamente la modalita' d'esecuzione
  char *modalita = malloc(MOD_LENGTH);
  if (strcmp(argv[1], "ARTIFICIALE") && (strcmp(argv[1], "NORMALE"))) {
    perror("ECU: invalid input modality");
    exit(EXIT_FAILURE);
  } else
    modalita = argv[1];

  /* FASE INIZIALIZZAZIONE SISTEMA
      Crea e inizializza tutti i processi figli salvando pid, pgid e comm_fd (cioe' file descriptor del 
      canale di comunicazione fra il processo e la ECU) del processo in una struct process
  */
  /* 
    Inizializzazione dei processi associati agli attuatori
    Nel gruppo degli attuatori non viene messo il processore di brake-by-wire perche' non viene terminato
    con gli altri attuatori subito dopo il viaggio ma appena prima della manovra di parcheggio cosi' che
    la macchina possa rallentare per fermarsi e effettuare il parcheggio
   */
  brake_process = brake_init();
  struct process steer_process = steer_init();
  struct process throttle_process = throttle_init(steer_process.pgid);
  processes_groups.actuators_group = steer_process.pgid;

  // Inizializzazione del processo associato a camera
  struct process camera_process = camera_init();
  // Come per gli attuatori salva il gruppo dei processi sensori in sensors_group
  processes_groups.sensors_group = camera_process.pgid;
  // Inizializza radar associandolo al gruppo dei sensori
  struct process radar_process = radar_init(modalita);

  // Apre, creandolo se necessario, il file di log della ECU dove verranno stampati i comandi
  if ((log_fd = openat(AT_FDCWD, "log/ECU.log", O_WRONLY | O_TRUNC | O_CREAT,
                       0644)) < 0)
    perror("openat ECU.log");

  /* 
    Viene inizializzato il signal handler che gestisce i seguenti segnali:
      SIGUSR1 -> throttle failure;
      SIGINT -> parcheggio completato;
    Infine SIGCHLD viene ignorato per ignorare la gestione dei figli
  */
  struct sigaction act = {0};
  act.sa_flags = SA_RESTART;
  act.sa_handler = &ECU_signal_handler;
  sigaction(SIGUSR1, &act, NULL);
  sigaction(SIGUSR2, &act, NULL);
  sigaction(SIGINT, &act, NULL);
  signal(SIGCHLD, SIG_IGN);

  // Alloca lo spazio per le stringhe dove salveremo le letture dei dati inviati
  // forward-facing-radar e da front-windshield-camera
  char *camera_buf = calloc(1, HMI_COMMAND_LENGTH);
  char *radar_buf = malloc(BYTES_CONVERTED);

  /*
    Inizializzazione della Human Machine Interface.
    Alloca lo spazio necessario per salvare i dati dei due processi, verifica se e' presente l'opzione
    "--term" e apre il terminale richiesto per l'output della hmi tramite lo script new_terminal
  */
  hmi_process = malloc(2 * sizeof(struct process));
  if (argc == 4 && !(strcmp(argv[2], "--term"))) {
    if (!(hmi_process[WRITE].pid = fork()))
      execlp("./sh/new_terminal.sh", "./sh/new_terminal.sh", argv[3], NULL);
  } else {
    if (!(hmi_process[WRITE].pid = fork()))
      execlp("./sh/new_terminal.sh", "./sh/new_terminal.sh", NULL);
  }
  // Inizializza i processi associati alla hmi sia in lettura che in scrittura
  hmi_init(hmi_process, 2);
  processes_groups.hmi_group = (hmi_process + READ)->pgid;

  /*
    Vengono inizializzate le ultime variabili necessarie per l'esecuzione corretta del programma:
     - travel_flag serve per eseguire il ciclo di viaggio in caso il programma riceva il comando INIZIO
                  e per parcheggiare direttamente nel caso in cui venga inserito PARCHEGGIO;
     - flag_arrest serve a decidere se uscire o no dal ciclo d'attesa iniziale in seguito a una stringa
                  d'input corretta (PARCHEGGIO o INIZIO);
     - hmi_command viene settato secondo il comando ricevuto dalla hmi-input cosi' da usare un intero e
                  non una stringa nel seguito del programma.
  */
  bool travel_flag = true;
  bool flag_arrest = true;
  unsigned short int hmi_command;

  // FINE INIZIALIZZAZIONE

  /*
    CICLO D'ATTESA
      Adesso inizia il ciclo in cui il programma attende l'arrivo di un input permesso leggendo dalla 
      hmi-input una volta al secondo.
      Se l'input ricevuto non e' corretto stampa a schermo un messaggio
      d'errore e attende un nuovo input
  */
  while (flag_arrest) {
    sleep(1);
    hmi_command = -1;
    read(hmi_process[READ].comm_fd, &hmi_command, sizeof(unsigned short int));
    switch (hmi_command) {
    case PARCHEGGIO:
      travel_flag = false;
    case INIZIO:
      flag_arrest = false;
      break;
    case ARRESTO:
      if (kill(hmi_process[READ].pid, SIGUSR2) < 0)
        perror("ECU: kill");
    default:
      break;
    }
  }

  /*
    PREPARAZIONE PER IL CICLO DI VIAGGIO
    Vengono definite il valore della velocita' richiesta dalla ECU che impostera' la velocita' tramite brake
    e throttle, una variabile temporanea per leggere il valore della velocita' mandata dalla 
    front-windshield-camera e un puntatore a file per aprire la pipe di camera 
  */
  int requested_speed = speed;
  int temp_v;
  FILE *camera_stream;
  if ((camera_stream = fdopen(camera_process.comm_fd, "r")) == NULL) {
    perror("ECU: fdopen camera");
  }

  if (kill(camera_process.pid, SIGUSR1) < 0) {
    perror("ECU: kill camera");
  }

  while (travel_flag) {
    read(radar_process.comm_fd, radar_buf, BYTES_CONVERTED);
    // legge dalla hmi esce arresta la macchina o esce dal ciclo del viaggio
    // per frenare e poi eseguire la procedura di parcheggio
    if (read(hmi_process[READ].comm_fd, &hmi_command, sizeof(short int)) > 0) {
      if (hmi_command == PARCHEGGIO)
        break;
      else if (hmi_command == ARRESTO)
        arrest(brake_process.pid);
      else
        kill(hmi_process[READ].pid, SIGUSR2);
    }
    // legge da front-windshield-camera e esegue l'azione opportuna:
    //	- esce dal ciclo per eseguire frenata e parcheggio;
    //	- arresta la macchina in caso di pericolo;
    //	- manda il comando della sterzata a steer-by-wire se deve girare;
    //	- se riceve un numero imposta la velocita' desiderata dalla camera;
    if (fgets(camera_buf, HMI_COMMAND_LENGTH, camera_stream) == NULL)
      perror("ECU: fgets camera");
    if (!strcmp(camera_buf, "PARCHEGGIO\n"))
      break;
    else if (!strcmp(camera_buf, "PERICOLO\n\0"))
      arrest(brake_process.pid);
    else if (!(strcmp(camera_buf, "DESTRA\n\0") &&
               strcmp(camera_buf, "SINISTRA\n\0"))) {
      send_command(steer_process.comm_fd, log_fd, hmi_process[WRITE].comm_fd,
                   camera_buf, strlen(camera_buf) + 1);
    } else if ((temp_v = atoi(camera_buf)) > 0)
      requested_speed = temp_v;
    sleep(1);

    // aggiorna la velocita' della macchina mandando un comando INCREMENTO 5 o
    // INCREMENTO 5 a throttle o brake per avvicinare di 5 la velocita' attuale
    // a quella desiderata
    if (speed != requested_speed)
      change_speed(requested_speed, throttle_process.comm_fd,
                   brake_process.comm_fd, log_fd, hmi_process[WRITE].comm_fd);
  }

  // 
  if (kill(-processes_groups.actuators_group, SIGKILL) < 0)
    perror("ECU: kill signal to actuators");
  
  if (kill(-processes_groups.sensors_group, SIGKILL) < 0)
    perror("ECU: kill signal to sensors");
  
  
  while (speed > 0) {
    change_speed(0, throttle_process.comm_fd, brake_process.comm_fd, log_fd,
                 hmi_process[WRITE].comm_fd);
    sleep(1);
  }

  if (kill(brake_process.pid, SIGKILL) < 0)
    perror("ECU: kill signal to brake");

  // avvia la procedura di parcheggio

  char park_data[BYTES_CONVERTED];
  park_process = park_assist_init(modalita);
  processes_groups.park_assist_group = park_process.pgid;
  sleep(1);

  write(park_process.comm_fd, "INIZIO\n\0", sizeof("INIZIO\n") + 1);

  // CICLO PARCHEGGIO
  while (true) {
    broad_log(hmi_process[WRITE].comm_fd, log_fd,
              "INIZIO PROCEDURA PARCHEGGIO\n\0",
              sizeof("INIZIO PROCEDURA PARCHEGGIO\n") + 1);

    int i = 1;
    while (true) {
      read(park_process.comm_fd, park_data, BYTES_CONVERTED);

      if (!acceptable_string(park_data) || !strcmp(park_data, "FINITO\n"))
        break;
      write(park_process.comm_fd, "CONTINUA\n\0", sizeof("CONTINUA\n") + 1);
    }

    if (!strcmp(park_data, "FINITO\n")) {
      broad_log(hmi_process[WRITE].comm_fd, log_fd,
                "PROCEDURA PARCHEGGIO COMPLETATA\n\0",
                sizeof("PROCEDURA PARCHEGGIO COMPLETATA\n") + 1);
      break;
    } else {
      write(park_process.comm_fd, "RIAVVIO\n\0", sizeof("RIAVVIO\n") + 1);
      broad_log(hmi_process[WRITE].comm_fd, log_fd,
                "ERRORE PARCHEGGIO, PROCEDURA INCOMPLETA\n\n\0",
                sizeof("ERRORE PARCHEGGIO, PROCEDURA INCOMPLETA\n\n") + 1);
      perror("ECU: riavvio parcheggio");
      read(park_process.comm_fd, NULL, PIPE_BUF);
    }
  }

  sleep(1);
  broad_log(hmi_process[WRITE].comm_fd, log_fd, "TERMINAZIONE PROGRAMMA\n\0",
            sizeof("TERMINAZIONE PROGRAMMA\n") + 1);

  // chiudo la socket con park_assist
  close(park_process.comm_fd);

  // termino i processi dei sensori, degli attuatori e di park-assist
  kill(park_process.pid, SIGKILL);
  kill(hmi_process[READ].pid, SIGKILL);

  return 0;
}

struct process brake_init() {
  struct process brake_process;
  brake_process.pid = make_process("brake-by-wire", 14, 0, NULL);
  brake_process.comm_fd = initialize_pipe("tmp/brake.pipe", O_WRONLY, 0666);
  brake_process.pgid = getpgid(brake_process.pid);
  return brake_process;
}

struct process steer_init() {
  struct process steer_process;
  steer_process.pid = make_process("steer-by-wire", 14, 0, NULL);
  steer_process.comm_fd = initialize_pipe("tmp/steer.pipe", O_WRONLY, 0666);
  steer_process.pgid = getpgid(steer_process.pid);
  return steer_process;
}

struct process throttle_init(pid_t actuator_group) {
  struct process throttle_process;
  throttle_process.pid =
      make_process("throttle-control", 17, actuator_group, NULL);
  throttle_process.pgid = actuator_group;
  throttle_process.comm_fd =
      initialize_pipe("tmp/throttle.pipe", O_WRONLY, 0666);
  return throttle_process;
}

struct process camera_init() {
  struct process camera_process;
  camera_process.pid = make_process("windshield-camera", 18, 0, NULL);
  camera_process.comm_fd = initialize_pipe("tmp/camera.pipe", O_RDONLY, 0666);
  camera_process.pgid = getpgid(camera_process.pid);
  return camera_process;
}

struct process radar_init(char *modalita) {
  struct process radar_process;
  radar_process.pid = make_sensor("RADAR", modalita, processes_groups.sensors_group);
  radar_process.comm_fd = initialize_pipe("tmp/radar.pipe", O_RDONLY, 0666);
  return radar_process;
}

void hmi_init(struct process *hmi_processes, int processes_number) {
  (hmi_processes + READ)->pid = make_process("hmi-input", 10, getpid(), NULL);
  (hmi_processes + READ)->comm_fd =
      initialize_pipe("tmp/hmi-in.pipe", O_RDONLY | O_NONBLOCK, 0666);
  (hmi_processes + WRITE)->comm_fd =
      initialize_pipe("tmp/hmi-out.pipe", O_WRONLY, 0666);
  (hmi_processes + READ)->pgid = getpgid((hmi_processes + READ)->pid);
  (hmi_processes + WRITE)->pgid = getpgid((hmi_processes + READ)->pid);
  return;
}

struct process park_assist_init(char *modalita) {
  struct process park_process;
  park_process.pid = make_process("park-assist", 12, 0, modalita);
  park_process.comm_fd = initialize_server_socket("./tmp/assist.sock");
  park_process.pgid = getpgid(park_process.pid);
  return park_process;
}

int initialize_server_socket(const char *socket_name) {
  int sock_fd;
  int cli_fd;
  unlink(socket_name);
  while ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror("ECU: socket function error");
    sleep(1);
  }
  struct sockaddr_un serv_addr;
  serv_addr.sun_family = AF_UNIX;
  strcpy(serv_addr.sun_path, socket_name);
  while (bind(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ECU: socket bind error");
    sleep(1);
  }
  if (listen(sock_fd, 1) < 0) {
    perror("ECU: socket listen error");
    sleep(1);
  }
  struct sockaddr cli_addr;
  socklen_t addr_len;
  while ((cli_fd = accept(sock_fd, &cli_addr, &addr_len)) < 0) {
    perror("ECU: accept function error");
    sleep(1);
  }
  return cli_fd;
}

void ECU_signal_handler(int sig) {
  if (sig == SIGUSR1) {
    // significa che ha fallito l'accelerazione
    arrest(brake_process.pid);
    broad_log(hmi_process[WRITE].comm_fd, log_fd, "TERMINAZIONE PROGRAMMA\n\0",
            sizeof("TERMINAZIONE PROGRAMMA\n") + 1);
    // TERMINO TUTTI I PROCESSI DOPO AVER ARRESTATO LA MACCHINA
    kill(-processes_groups.actuators_group, SIGKILL);
    kill(-brake_process.pid, SIGKILL);
    kill(-processes_groups.sensors_group, SIGKILL);
    kill(-processes_groups.hmi_group, SIGUSR1);
    exit(EXIT_FAILURE);
  } else if (sig == SIGINT) {
    // significa che park_assist ha concluso la procedura di parcheggio
    if (kill(-processes_groups.actuators_group, SIGKILL) < 0)
      perror("ECU: kill actuators");
    kill(-processes_groups.sensors_group, SIGKILL);
    kill(-processes_groups.park_assist_group, SIGKILL);
    kill(-processes_groups.hmi_group, SIGKILL);
    umask(022);
    close(park_process.comm_fd);
    exit(EXIT_SUCCESS);
  }
}

void arrest(pid_t brake_process) {
  broad_log(hmi_process[WRITE].comm_fd, log_fd, "ARRESTO AUTO\n\0",
            sizeof("ARRESTO AUTO\n") + 1);
  kill(brake_process, SIGUSR1);
  speed = 0;
}

void send_command(int actuator_pipe_fd, int log_fd, int hmi_out_fd,
                  char *command, size_t command_size) {
  broad_log(actuator_pipe_fd, log_fd, command, command_size);
  write(hmi_out_fd, command, command_size);
}

void change_speed(int requested_speed, int throttle_pipe_fd, int brake_pipe_fd,
                  int log_fd, int hmi_pipe_fd) {
  if (requested_speed > speed) {
    send_command(throttle_pipe_fd, log_fd, hmi_pipe_fd, "INCREMENTO 5\n",
                 sizeof("INCREMENTO 5\n"));
    speed += 5;
  } else if (requested_speed < speed) {
    send_command(brake_pipe_fd, log_fd, hmi_pipe_fd, "FRENO 5\n",
                 sizeof("FRENO 5\n"));
    speed -= 5;
  }
}

bool acceptable_string(char *string) {
  if (strstr(string, "172A") != NULL || (strstr(string, "D693") != NULL) ||
      strstr(string, "0000") != NULL || strstr(string, "BDD8") != NULL ||
      strstr(string, "FAEE") != NULL || strstr(string, "4300") != NULL)
    return false;
  return true;
}
