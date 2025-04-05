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
#include <math.h>

#define DEFAULT_HEIGHT 10
#define DEFAULT_WIDTH 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 10
#define ERROR_VALUE -1
#define GAME_STATE_SHM_NAME "/game_state"

#define MAX_PLAYERS 9

#define WRITE 0
#define READ 1

typedef struct {
    int width;
    int height;
    int delay;
    int timeout;
    int seed;
    char* view_path; // puede ser NULL
    char* playerPaths[MAX_PLAYERS];
    int playerCount;
} config_t;

void * createGameStateSHM(char * name, config_t * config);
int deleteGameStateSHM(char * name, gameStateSHMStruct * gameState);
int clearPipes(int playerCount, int (*pipeMasterToPlayer)[2], int (*pipePlayerToMaster)[2]);

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
  config.playerCount = 0;

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
                config.playerPaths[0] = strdup(optarg);
                config.playerCount = 1;
                for (int i = optind; i < argc ; i++) {
                  if(config.playerCount >= MAX_PLAYERS){
                    fprintf(stderr, "Como mucho debe especificar 9 jugadores usando -p\n");
                    exit(EXIT_FAILURE);
                  }
                    config.playerPaths[config.playerCount++] = strdup(argv[i]);
                    optind++;
                }
                break;
            default:
                fprintf(stderr, "Uso: %s [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] -p player1 player2 ...\n", argv[0]);
                exit(EXIT_FAILURE);
        }
  }

  if (config.playerCount == 0) {
       fprintf(stderr, "Debe especificar al menos un jugador con -p\n");
       return -1;
  }

  //Crear las memorias compartidas
  gameStateSHMStruct * gameStateSHM = (gameStateSHMStruct * ) createGameStateSHM(GAME_STATE_SHM_NAME, &config);

  //Asigno la informacion a la memoria compartida
  gameStateSHM->gameState = false;
  gameStateSHM->playerQty = config.playerCount;
  gameStateSHM->tableWidth = config.width;
  gameStateSHM->tableHeight = config.height;

  //Crear los valores del tablero:
  int *tablero = gameStateSHM->tableStartPointer;
    for (int i = 0; i < config.width * config.height; i++) {
      tablero[i] = (rand() % 9) + 1; // recompensa entre 1 y 9
  }

  int pipePlayerToMaster[config.playerCount][2];
  int pipeMasterToPlayer[config.playerCount][2];
  pid_t playerPids[config.playerCount];

  for (int i = 0; i < config.playerCount; i++){
    printf(config.playerPaths[i]);
  }

  //Crear separacion de jugadores
  int width = gameStateSHM->tableWidth;
  int height = gameStateSHM->tableHeight;
  int playerQty = gameStateSHM->playerQty;

  int rows = (int) sqrt(playerQty);
  int columns = (playerQty + rows - 1) / rows;

  int cellWidth = width / columns;
  int cellHeight = height / rows;
  
  for (int i = 0; i < config.playerCount; i++) {

    playerSHMStruct * player = &gameStateSHM->playerList[i];
    strncpy(player->playerName, config.playerPaths[i], sizeof(player->playerName) - 1);
    player->playerName[sizeof(player->playerName) - 1] = '\0';

    int row = i / columns;
    int col = i % columns;

    int x_ini = col * cellWidth;
    int y_ini = row * cellHeight;

    int x = x_ini + cellWidth / 2;
    int y = y_ini + cellHeight / 2;

    if (x >= width) x = width - 1;
    if (y >= height) y = height - 1;

    player->score = 0;
    player->invalidMoveQty = 0;
    player->validMoveQty = 0;
    player->tableX = x;
    player->tableY = y; 
    player->isBlocked = false;

    //Creamos pipes de player a master
    if (pipe(pipePlayerToMaster[i]) == ERROR_VALUE) {
      perror("Error in pipe player to master");
      exit(EXIT_FAILURE);
    }

    //Creamos pipes de master a player
    if (pipe(pipeMasterToPlayer[i]) == ERROR_VALUE) {
      perror("Error in pipe master to player");
      exit(EXIT_FAILURE);
    }

    //Creamos procesos hijos
    pid_t pid = fork();
    if (pid == ERROR_VALUE) {
      perror("fork");
      exit(EXIT_FAILURE);
    }
    
    if(pid == 0){
      close(pipePlayerToMaster[i][0]); // Cierra lectura
      dup2(pipePlayerToMaster[i][1], STDOUT_FILENO); // Redirige stdout al pipe
      // Armo argumentos
      char width_str[10], height_str[10];
      snprintf(width_str, sizeof(width_str), "%d", config.width);
      snprintf(height_str, sizeof(height_str), "%d", config.height);

      char *argumentos[] = { config.playerPaths[i], width_str, height_str, NULL };

      execve(config.playerPaths[i], argumentos, NULL);

      // Solo si execve falla:
      perror("execve");
      exit(EXIT_FAILURE);
    }
  }



  //Borrar las memorias compartidas
  if(deleteGameStateSHM(GAME_STATE_SHM_NAME,gameStateSHM) == ERROR_VALUE){
    exit(EXIT_FAILURE);
  }
  //Limpio los pipes
  if(clearPipes(config.playerCount, pipePlayerToMaster, pipeMasterToPlayer) == ERROR_VALUE){
    exit(EXIT_FAILURE);
  }
  //Esperamos a los hijos para terminar
  for (int i = 0; i < config.playerCount; i++) {
    int status;
    waitpid(playerPids[i], &status, 0);
  }

  // for (int y = 0; y < height; y++) {
  //   for (int x = 0; x < width; x++) {
  //       int index = y * width + x;
  //       int cell = gameStateSHM->tableStartPointer[index-1];

  //       // Print cell with formatting
  //       if (cell == 0) {
  //           printf(" . ");      // Empty cell
  //       } else if (cell > 0) {
  //           printf(" %d ", cell); // Reward
  //       } else {
  //           printf("P%d ", -cell); // Player (negative values)
  //       }
  //   }
  //   printf("\n");
  // }


}

//Shared Memory of gameState

void * createGameStateSHM(char * name, config_t * config) {
    shm_unlink(name);
       
    int fd;
    fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    
    if(fd == ERROR_VALUE) {
        perror("Fallo al crear la memoria compartida del game state\n");
        return NULL;
    }

    size_t totalSize = sizeof(gameStateSHMStruct) + sizeof(int) * config->width * config->height;
    
    if(ftruncate(fd, totalSize) == ERROR_VALUE) {
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
    gameState->playerQty = config->playerCount;
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

gameSyncSHMStruct * createGameSyncSHM(char * name) {
    shm_unlink(name);
       
    int fd;
    fd = shm_open(name, O_CREAT | O_RDWR | O_TRUNC, 0644);
    
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

int clearPipes(int playerCount, int (*pipeMasterToPlayer)[2], int (*pipePlayerToMaster)[2]){
  for (int i = 0; i < playerCount; i++){
    //Limpio primer pipe de master a player
    if(close(pipeMasterToPlayer[i][WRITE]) == ERROR_VALUE){
      perror("Fallo limpiar el pipe de master a player");
      return ERROR_VALUE;
    }
    //Limpio segundo pipe de player a master
      if(close(pipePlayerToMaster[i][READ]) == ERROR_VALUE){
      perror("Fallo limpiar el pipe de player a master");
      return ERROR_VALUE;
    }
  }
  return 0;
}



