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

// struttura per salvare i processi con i loro pid, il gruppo a cui appartengono
// e la pipe con cui comunicare col processo. brake_process e' globale perche'
// serve avere il pid di brake_process nell'ECU_signal_handler
struct process {
  pid_t pid;
  pid_t pgid;
  int comm_fd;
} brake_process, park_process;

// struttura globale che serve per chiudere tutti i processi che vengono
// aperti durante l'esecuzione
struct groups {
  pid_t park_assist_group;
  pid_t actuators_group;
  pid_t sensors_group;
  pid_t hmi_group;
} processes_groups;

struct process brake_init();
struct process steer_init(pid_t);
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

struct process *hmi_process;

int speed = 0;
int log_fd;
bool park_done_flag = false;
long int string_search;

int main(int argc, char **argv) {
  // apre un file di log per gli errori che sara' condiviso da tutti i processi
  // figli
  umask(0000);
  // unlink("../log/errors.log");
  int error_log_fd =
      openat(AT_FDCWD, "log/errors.log", O_WRONLY | O_TRUNC | O_CREAT, 0666);
  dup2(error_log_fd, STDERR_FILENO);

  // controlla che gli argomenti da linea di comando siano 2 o 4 per accettare
  // la modalita' d'esecuzione e nel caso il terminale da usare per la hmi
  if (argc < 2 || argc == 3 || argc > 4) {
    perror("ECU: syntax error");
    exit(EXIT_FAILURE);
  }

  // controlla che il secondo argomento sia la modalita' d'esecuzione
  char *modalita = malloc(MOD_LENGTH);
  if (strcmp(argv[1], "ARTIFICIALE") && (strcmp(argv[1], "NORMALE"))) {
    perror("ECU: invalid input modality");
    exit(EXIT_FAILURE);
  } else
    modalita = argv[1];

  // FASE INIZIALIZZAZIONE SISTEMA

  // inizializza tutti i processi figli salvando dove necessario l'intero
  // processo in una struct process (nel caso di brake-by-wire,
  // windshield-camera e park-assist) mentre negli altri casi vengono salvati
  // solo i file descriptor della pipe che serve per comunicare con il nuovo
  // processo
  brake_process = brake_init();
  struct process steer_process = steer_init(brake_process.pgid);
  struct process throttle_process = throttle_init(brake_process.pgid);
  processes_groups.actuators_group = brake_process.pgid;

  struct process camera_process = camera_init();
  pid_t sensor_signal = -camera_process.pgid;
  struct process radar_process = radar_init(modalita);
  processes_groups.sensors_group = camera_process.pgid;

  if ((log_fd = openat(AT_FDCWD, "log/ECU.log", O_WRONLY | O_TRUNC | O_CREAT,
                       0644)) < 0)
    perror("openat ECU.log");

  // attivo il signal handler per l'errore nell'accelerazione e la conclusione
  // di parcheggio

  struct sigaction act = {0};
  act.sa_flags = SA_RESTART;
  act.sa_handler = &ECU_signal_handler;
  sigaction(SIGUSR1, &act, NULL);
  sigaction(SIGUSR2, &act, NULL);
  sigaction(SIGINT, &act, NULL);
  signal(SIGCHLD, SIG_IGN);

  char *camera_buf = calloc(1, HMI_COMMAND_LENGTH);
  char *radar_buf = malloc(BYTES_CONVERTED);

  // preparo il segnale da mandare ai sensori quando dovranno essere disattivati
  // per la procedura di parcheggio
  pid_t parking_signal = -brake_process.pgid;

  // se c'e' imposta il terminale scelto dall'utente
  hmi_process = malloc(2 * sizeof(struct process));
  if (argc == 4 && !(strcmp(argv[2], "--term"))) {
    if (!(hmi_process[WRITE].pid = fork()))
      execlp("./sh/new_terminal.sh", "./sh/new_terminal.sh", argv[3], NULL);
  } else {
    if (!(hmi_process[WRITE].pid = fork()))
      execlp("./sh/new_terminal.sh", "./sh/new_terminal.sh", NULL);
  }
  hmi_init(hmi_process, 2);
  processes_groups.hmi_group = (hmi_process + READ)->pgid;

  // questa variabile mi serve nel caso venga chiamato PARCHEGGIO da hmi prima
  // che venga digitato INIZIO. In questo caso diventa false e l'auto viene
  // parcheggiata immediata senza iniziare il viaggio
  bool travel_flag = true;
  bool flag_arrest = true;
  unsigned short int hmi_command;
  // ciclo d'attesa prima dell'inizio del viaggio
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
    printf("actual speed: %d \tcommand recived: %s", speed, camera_buf);

    // aggiorna la velocita' della macchina mandando un comando INCREMENTO 5 o
    // INCREMENTO 5 a throttle o brake per avvicinare di 5 la velocita' attuale
    // a quella desiderata
    if (speed != requested_speed)
      change_speed(requested_speed, throttle_process.comm_fd,
                   brake_process.comm_fd, log_fd, hmi_process[WRITE].comm_fd);
  }

  // frena per azzerare la velocita' e cosi' poter avviare la procedura di
  // parcheggio
  while (speed > 0) {
    change_speed(0, throttle_process.comm_fd, brake_process.comm_fd, log_fd,
                 hmi_process[WRITE].comm_fd);
    sleep(1);
  }

  // avvia la procedura di parcheggio
  if (kill(parking_signal, SIGINT) < 0)
    perror("ECU: kill parking signal");
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

      printf("Lettura numero:%2d\t%s", i++, park_data);
      if (!acceptable_string(park_data) || !strcmp(park_data, "FINITO\n")) {
        printf("ECU: unacceptable input: string found %ld\n", string_search);
        break;
      }
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

    /*
    park_done_flag = false;
    int i = 1;
    int odd = 0;
    char * buff = malloc(PIPE_BUF);
    read(park_process.comm_fd, buff, PIPE_BUF);
    free(buff);
    sleep(2);

    while( !park_done_flag ) {
            read(park_process.comm_fd, park_data, BYTES_CONVERTED);
            printf("Lettura numero:%2d\t%s", i++, park_data);
if(!acceptable_string(park_data)){
                    printf("ECU: unacceptable input: string found %ld\n",
string_search); break;
            }
            odd %= 2;
            if ( odd++ == 1 )
                    sleep(1);
}

    if(park_done_flag){
            broad_log(hmi_process[WRITE].comm_fd, log_fd, "PROCEDURA PARCHEGGIO
COMPLETATA\n\0", sizeof("PROCEDURA PARCHEGGIO COMPLETATA\n")+1); break; } else {
kill(park_process.pid, SIGUSR2);
            broad_log(hmi_process[WRITE].comm_fd, log_fd, "ERRORE PARCHEGGIO,
PROCEDURA INCOMPLETA\n\n\0", sizeof("ERRORE PARCHEGGIO, PROCEDURA
INCOMPLETA\n\n")+1); perror("ECU: signaling park to RESTART");
    }*/
  }

  sleep(1);
  broad_log(hmi_process[WRITE].comm_fd, log_fd, "TERMINAZIONE PROGRAMMA\n\0",
            sizeof("TERMINAZIONE PROGRAMMA\n") + 1);

  // chiudo la socket con park_assist
  close(park_process.comm_fd);

  // termino i processi dei sensori, degli attuatori e di park-assist
  kill(park_process.pid, SIGINT);
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

struct process steer_init(pid_t actuator_group) {
  struct process steer_process;
  steer_process.pid =
      make_process("steer-by-wire", 14, brake_process.pgid, NULL);
  setpgid(steer_process.pid, actuator_group);
  steer_process.pgid = actuator_group;
  steer_process.comm_fd = initialize_pipe("tmp/steer.pipe", O_WRONLY, 0666);
  return steer_process;
}

struct process throttle_init(pid_t actuator_group) {
  struct process throttle_process;
  throttle_process.pid =
      make_process("throttle-control", 17, brake_process.pgid, NULL);
  throttle_process.pgid = setpgid(throttle_process.pid, actuator_group);
  throttle_process.comm_fd =
      initialize_pipe("tmp/throttle.pipe", O_WRONLY, 0666);
  return throttle_process;
}

struct process camera_init() {
  struct process camera_process;
  camera_process.pid =
      make_process("windshield-camera", 18, brake_process.pgid, NULL);
  camera_process.comm_fd = initialize_pipe("tmp/camera.pipe", O_RDONLY, 0666);
  camera_process.pgid = getpgid(brake_process.pid);
  setpgid(camera_process.pid, brake_process.pgid);
  return camera_process;
}

struct process radar_init(char *modalita) {
  struct process radar_process;
  radar_process.pid = make_sensor("RADAR", modalita, brake_process.pgid);
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
    // TERMINO TUTTI I PROCESSI DOPO AVER ARRESTATO LA MACCHINA
    // NON IMPORTA UCCIDERE PARK ASSIST PERCHE' E' GIA' TERMINATO
    kill(-(processes_groups.actuators_group), SIGKILL);
    kill(-(processes_groups.sensors_group), SIGKILL);
    kill(-(processes_groups.hmi_group), SIGUSR1);
  } else if (sig == SIGUSR2)
    park_done_flag = true;
  else if (sig == SIGINT) {
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
  string_search = 1;
  if (strstr(string, "5152") != NULL)
    string_search *= 2;
  if (strstr(string, "D693") != NULL)
    string_search *= 3;
  if (strstr(string, "0000") != NULL)
    string_search *= 5;
  if (strstr(string, "BDD8") != NULL)
    string_search *= 7;
  if (strstr(string, "FAEE") != NULL)
    string_search *= 11;
  if (strstr(string, "4300") != NULL)
    string_search *= 13;
  return (bool)(string_search == 1);

  // return (bool) ((strstr(string,"5152") == NULL) && (strstr(string,"D693") ==
  // NULL) && (strstr(string,"0000") == NULL) && (strstr(string,"BDD8") == NULL)
  // && (strstr(string,"FAEE") == NULL) && (strstr(string,"4300") == NULL));
}
