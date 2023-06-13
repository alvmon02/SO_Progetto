// #include <stdio.h>
// #include <stdlib.h>
// #include <fcntl.h>
// #include <unistd.h>
// #include "../../include/service-functions.h"
//
// // MACROS
// // INPUT_LEN: numero fisso di byte da leggere dall'input
// #define INPUT_LEN 8
//
// // File descriptor del log file
// short int log_fd;
// // File descriptor del pipe in scrittura
// short int pipe_fd;
//
// // La funzione main esegue le operazioni relative al componente
// // forward-facing-radar
// int main ( ) {
//
//   // Connessione del file descriptor del pipe per la comunicazione tra central
//   // ECU e foward-facing-radar. Il protocollo impone che il pipe sia creato
//   // durante la fase di inizializzazione del processo central-ECU e che venga
//   // aperto in sola scrittura dal processo foward-facing-radar il quale vi
//   // scriva (quasi) una volta al secondo.
//   pipe_fd = open("../tmp/radar.pipe", O_WRONLY);
//   // Connessione del file descriptor del log file. Apertura in sola scrittura.
//   // Qualora il file non esista viene creato. Qualora il file sia presente
//   // si mantengono le precedenti scritture.
//   log_fd = open("../log/radar.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
//   // Connessione del file descriptor al file da cui ottenere i dati di input.
//   // Apertura in sola lettura all'inizio del file.
//   short int input_fd = open("/dev/urandom", O_RDONLY, 0400);
//   // Inizializzazione della stringa di input che rappresenta l'insieme di byte
//   // da trasmettere alla central-ECU da parte del processo.
//   // Il +1 e` necessario per inserirvi il carattere di terminazione riga '\n'.
//   char radar_input[INPUT_LEN + 1];
//   radar_input[INPUT_LEN] = '\n';
//
//
//   // Il ciclo infinito successivo rappresenta il cuore del processo.
//   // Ad ogni lettura del file di input accadrà che il file non
//   // raggiungera mai la terminazione e si controllera che il numero di
//   // byte letti sia effettivamente 8 (= INPUT_LEN). Se cosi non fosse
//   // si salta quella lettura senza inviare dati ne' loggarli.
//   // Viene eseguita una lettura al secondo e un numero di invii
//   // inferiore in caso di incompletezza delle letture.
//   while (1) {
//     if ((read(input_fd, radar_input, INPUT_LEN)) == INPUT_LEN){
//       broad_log(pipe_fd, log_fd, radar_input, INPUT_LEN+1);
//     }
//     sleep(1);
//   }
// }
