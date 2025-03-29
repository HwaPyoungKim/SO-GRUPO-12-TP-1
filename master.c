#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mman.h>
#include "sharedMemory.h"
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define DEFAULT_HEIGHT 10
#define DEFAULT_WIDTH 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 10
#define ERROR_VALUE -1
#define SHARED_BETWEEN_PROCESSES 1

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

// void * createPlayerSHM(char * name, size_t size){
//   int fd;
//   fd = shm_open(name, O_RDWR | O_CREAT, 0666); // mode solo para crearla
//   if(fd == -1){
//     perror("shm_open");
//     exit(EXIT_FAILURE);
//   }

//   //solo para crearla
//   if(-1 == ftruncate(fd,size)){
//     perror("ftruncate");
//     exit(EXIT_FAILURE);
//   }

//   void *p = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
//   if(p == MAP_FAILED){
//     perror("mmap");
//     exit(EXIT_FAILURE);
//   }
//   return p;
// }

int
main(int argc, char *argv[]) {
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
        return -1;
    }

  

////////////////////////////////////////////////////////////////////////
}

//Shared Memory of gameState

void * createGameStateSHM(char * name, config_t * config) {
    shm_unlink(name);
       
    int fd;
    fd = shm_open(name, O_CREAT | O_RDWR | O_TRUNC, 0666);
    
    if(fd == ERROR_VALUE) {
        perror("Fallo al crear la memoria compartida del game state\n");
        return NULL;
    }

    size_t totalSize = sizeof(gameStateSHMStruct) + sizeof(int) * config->width * config->height;
    
    if(ftruncate(fd, sizeof(totalSize)) == ERROR_VALUE) {
        perror("No hay espacio para la memoria compartida del game state\n");
        return NULL;
    }

    gameStateSHMStruct * gameState = (gameStateSHMStruct *) mmap(NULL, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if(gameState == MAP_FAILED) {
        perror("Fallo mapear la memoria compartida del game state\n");
        return NULL;
    }

    if(close(fd) == ERROR_VALUE) {
      perror("Fallo cerrar la memoria compartida del game state\n");
      return NULL;
    }

    gameState->tableWidth = config->width;
    gameState->tableHeight = config->height;
    gameState->playerQty = config->player_count;
    gameState->gameState = false;

    return gameState;
}

int deleteGameStateSHM(char * name, gameStateSHMStruct * gameState) {

    if(munmap(gameState, sizeof(gameStateSHMStruct)) == ERROR_VALUE) {
      perror("Fallo desmapear la memoria compartida del game state\n");
      return ERROR_VALUE;
    }

    if(shm_unlink(name) == ERROR_VALUE){
      perror("Fallo deslinkear la memoria compartida del game state\n");
      return ERROR_VALUE;
    }
    return 0;
}

//Shared Memory of gameSync

gameSyncSHMStruct * createGameSyncSHM(char * name, int total_files) {
    shm_unlink(name);
       
    int fd;
    fd = shm_open(name, O_CREAT | O_RDWR | O_TRUNC, 0666);
    
    if(fd == ERROR_VALUE) {
        perror("Fallo al crear la memoria compartida del game sync\n");
        return NULL;
    }
    
    if(ftruncate(fd, sizeof(gameSyncSHMStruct)) == ERROR_VALUE) {
        perror("No hay espacio para la memoria compartida del game Sync\n");
        return NULL;
    }

    gameSyncSHMStruct * gameSync = mmap(NULL, sizeof(gameSyncSHMStruct), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if(gameSync == MAP_FAILED) {
        perror("Fallo mapear la memoria compartida del game sync\n");
        return NULL;
    }

    if(sem_init(&(gameSync->A), MAP_SHARED, 0) == ERROR_VALUE) {
        perror("Fallo crear el semaforo A\n");
        return NULL;
    }

    if(sem_init(&(gameSync->B), MAP_SHARED, 0) == ERROR_VALUE) {
      perror("Fallo crear el semaforo B\n");
      return NULL;
    }

    if(sem_init(&(gameSync->C), MAP_SHARED, 0) == ERROR_VALUE) {
      perror("Fallo crear el semaforo C\n");
      return NULL;
    }

    if(sem_init(&(gameSync->D), MAP_SHARED, 0) == ERROR_VALUE) {
      perror("Fallo crear el semaforo D\n");
      return NULL;
    }

    if(sem_init(&(gameSync->E), MAP_SHARED, 0) == ERROR_VALUE) {
      perror("Fallo crear el semaforo E\n");
      return NULL;  
    }

    if(close(fd) == ERROR_VALUE) {
        perror("Fallo cerrar la memoria compartida del game sync\n");
        return NULL;
    }

    //guardar las cosas

    return gameSync;
}

int deleteGameSyncSHM(char * name, gameSyncSHMStruct * gameSync) {

    if(sem_destroy(&(gameSync->A)) == ERROR_VALUE) {
      perror("Fallo cerrar el semaforo A");
      return ERROR_VALUE;
    }

    if(sem_destroy(&(gameSync->B)) == ERROR_VALUE) {
      perror("Fallo cerrar el semaforo B");
      return ERROR_VALUE;
    }

    if(sem_destroy(&(gameSync->C)) == ERROR_VALUE) {
      perror("Fallo cerrar el semaforo C");
      return ERROR_VALUE;
    }

    if(sem_destroy(&(gameSync->D)) == ERROR_VALUE) {
      perror("Fallo cerrar el semaforo D");
      return ERROR_VALUE;
    }

    if(sem_destroy(&(gameSync->E)) == ERROR_VALUE) {
      perror("Fallo cerrar el semaforo E");
      return ERROR_VALUE;
    }

    if(munmap(gameSync, sizeof(gameSyncSHMStruct)) == ERROR_VALUE) {
        perror("Fallo desmapear la memoria compartida del game sync\n");
        return ERROR_VALUE;
    }

    if(shm_unlink(name) == ERROR_VALUE){
        perror("Fallo deslinkear la memoria compartida del game sync");
        return ERROR_VALUE;
    }
    return 0;
}