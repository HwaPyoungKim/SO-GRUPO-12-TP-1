#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mman.h>
#include "shareMemory.h"
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define DEFAULT_HEIGHT 10
#define DEFAULT_WIDTH 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 10

#define MAX_PLAYERS 9

typedef struct {
    int width;
    int height;
    int delay;
    int timeout;
    int seed;
    char* view_path; // puede ser NULL
    char* player_paths[MAX_PLAYERS];
    int player_count;
} config_t;


//Funcion para crear 2 memorias compartidas “/game_state” y “/game_sync”

void * createPlayerSHM(char * name, size_t size){
  int fd;
  fd = shm_open(name, O_RDWR | O_CREAT, 0666); // mode solo para crearla
  if(fd == -1){
    perror("shm_open");
    exit(EXIT_FAILURE);
  }

  //solo para crearla
  if(-1 == ftruncate(fd,size)){
    perror("ftruncate");
    exit(EXIT_FAILURE);
  }

  void *p = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
  if(p == MAP_FAILED){
    perror("mmap");
    exit(EXIT_FAILURE);
  }
  return p;
}

int
main(int argc, char *argv[])
{
        //validacion de parametros que se le pasa al master
//      ● [-w width]: Ancho del tablero. Default y mínimo: 10
//      ● [-h height]: Alto del tablero. Default y mínimo: 10
//      ● [-d delay]: milisegundos que espera el máster cada vez que se imprime el estado. Default: 200
//      ● [-t timeout]: Timeout en segundos para recibir solicitudes de movimientos válidos. Default: 10
//      ● [-s seed]: Semilla utilizada para la generación del tablero. Default: time(NULL)
//      ● [-v view]: Ruta del binario de la vista. Default: Sin vista.
//      ● -p player1 player2: Ruta/s de los binarios de los jugadores. Mínimo: 1, Máximo: 9
//./CHomdasdads -h 200 -s 2000

  config_t config;

  config.width = DEFAULT_WIDTH;
  config.height = DEFAULT_HEIGHT;
  config.delay = DEFAULT_DELAY;
  config.timeout = DEFAULT_TIMEOUT;
  config.seed = time(NULL);
  config.view_path = NULL;
  config.player_count = 0;

  int opt;

  while ((opt = getopt(argc, argv, "w:h:d:t:s:v:p:")) != -1) {
        switch (opt) {
            case 'w':
                config.width = atoi(optarg);
                if (config.width < 10) config.width = 10;
                break;
            case 'h':
                config.height = atoi(optarg);
                if (config.height < 10) config.height = 10;
                break;
            case 'd':
                config.delay = atoi(optarg);
                break;
            case 't':
                config.timeout = atoi(optarg);
                break;
            case 's':
                config.seed = atoi(optarg);
                break;
            case 'v':
                config.view_path = strdup(optarg);
                break;
            case 'p':
                // Los jugadores vienen después de -p
                config.player_paths[0] = strdup(optarg);
                config.player_count = 1;
                for (int i = optind; i < argc ; i++) {
                  if(config.player_count >= MAX_PLAYERS){
                    fprintf(stderr, "Como mucho debe especificar 9 jugadores usando -p\n");
                    exit(EXIT_FAILURE);
                  }
                    config.player_paths[config.player_count++] = strdup(argv[i]);
                    optind++;
                }
                return 0; // ya no hay más argumentos relevantes
            default:
                fprintf(stderr, "Uso: %s [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] -p player1 player2 ...\n", argv[0]);
                exit(EXIT_FAILURE);
        }
  }

  if (config.player_count == 0) {
        fprintf(stderr, "Debe especificar al menos un jugador con -p\n");
        exit(EXIT_FAILURE);
  }

  // int    pipefd[2];
  // char   buf;
  // pid_t  cpid;

  // if (argc != 2) {
  //   fprintf(stderr, "Usage: %s <string>\n", argv[0]);
  //   exit(EXIT_FAILURE);
  // }

  // if (pipe(pipefd) == -1) {
  //   perror("pipe");
  //   exit(EXIT_FAILURE);
  // }

  // cpid = fork();
  // if (cpid == -1) {
  //   perror("fork");
  //   exit(EXIT_FAILURE);
  // }

  // if (cpid == 0) {    /* Child reads from pipe */
  //   close(pipefd[1]);          /* Close unused write end */

  //   while (read(pipefd[0], &buf, 1) > 0)
  //     write(STDOUT_FILENO, &buf, 1);

  //   write(STDOUT_FILENO, "\n", 1);
  //   close(pipefd[0]);
  //   _exit(EXIT_SUCCESS);

  // } else {            /* Parent writes argv[1] to pipe */
  //   close(pipefd[0]);          /* Close unused read end */
  //   write(pipefd[1], argv[1], strlen(argv[1]));
  //   close(pipefd[1]);          /* Reader will see EOF */
  //   wait(NULL);                /* Wait for child */
  //   exit(EXIT_SUCCESS);
  // }
}