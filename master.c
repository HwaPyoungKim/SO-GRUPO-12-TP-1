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
#define GAME_SYNC_SHM_NAME "/game_sync"

#define LIMITMOVEINTERIOR 8
#define LIMITMOVEBORDER 5
#define LIMITMOVECORNER 3

#define MAX_PLAYERS 9

#define WRITE 0
#define READ 1

// typedef enum {
//   NORTH, NORTHEAST, EAST, SOUTHEAST, SOUTH, SOUTHWEST, WEST, NORTHWEST
// } Direction;

typedef struct {
    int width;
    int height;
    int delay;
    int timeout;
    int seed;
    char * view_path; // puede ser NULL
    char * playerPaths[MAX_PLAYERS];
    int playerCount;
} config_t;

void * createGameStateSHM(char * name, config_t * config);
void * createGameSyncSHM(char * name);
int deleteGameStateSHM(char * name, gameStateSHMStruct * gameState);
int clearPipes(int playerCount, int (*pipePlayerToMaster)[2]);
bool checkMovement(int indexPlayer, gameStateSHMStruct * gameStateSHM, unsigned char mov);
void printTablero(int * table, int width, int height);
bool validMove(unsigned char mov, int indexPlayer, gameStateSHMStruct * gameStateSHM);
bool isBorder(int indexPlayer, gameStateSHMStruct * gameStateSHM);
bool checkMoveAvailability(int indexPlayer, gameStateSHMStruct * gameStateSHM);
void movCases(int mov, int * posX, int * posY);
bool checkIfOccupied(int posTable, gameStateSHMStruct * gameStateSHM);
void beginWrite(gameSyncSHMStruct * gameSyncSHM);
void endWrite(gameSyncSHMStruct * gameSyncSHM);

int
main(int argc, char * argv[]) {
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
                if (config.width < 10) config.width = 10; // ver si chequear que no nos pasen algo menor a 10
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
                  if (argv[i][0] == '-') {
                    break;
                  }
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
  gameStateSHMStruct * gameStateSHM = (gameStateSHMStruct *) createGameStateSHM(GAME_STATE_SHM_NAME, &config);

  //Crear los semaforos
  gameSyncSHMStruct * gameSyncSHM = (gameSyncSHMStruct *) createGameSyncSHM(GAME_SYNC_SHM_NAME);

  //Crear los valores del tablero:
  int * tablero = gameStateSHM->tableStartPointer;
    for (int i = 0; i < config.width * config.height; i++) {
      tablero[i] = (rand() % 9) + 1; // recompensa entre 1 y 9
  }

  //Creo el fork a la vista
  if(config.view_path != NULL){
    pid_t pid = fork();
    if (pid == ERROR_VALUE) {
      perror("fork");
      exit(EXIT_FAILURE);
    }

    if(pid == 0){
      //Pipes para vista

      // Armo argumentos
      char width_str[10], height_str[10];
      snprintf(width_str, sizeof(width_str), "%d", config.width);
      snprintf(height_str, sizeof(height_str), "%d", config.height);

      char * arguments[] = { config.view_path, width_str, height_str, NULL };

      execve(config.view_path, arguments, NULL);

      // Solo si execve falla:
      perror("execve");
      exit(EXIT_FAILURE);
    }
  }

  int pipePlayerToMaster[config.playerCount][2];
  pid_t playerPids[config.playerCount];

  // for (int i = 0; i < config.playerCount; i++){
  //   printf(config.playerPaths[i]);
  // }

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

    // asignamos la posicion inicial de cada jugador segun los calculos de la funcion
    gameStateSHM->tableStartPointer[y * width + x] = (i + 1) * (-1); 

    //Creamos pipes de player a master
    if (pipe(pipePlayerToMaster[i]) == ERROR_VALUE) {
      perror("Error in pipe player to master");
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

      char *arguments[] = { config.playerPaths[i], width_str, height_str, NULL };

      execve(config.playerPaths[i], arguments, NULL);

      // Solo si execve falla:
      perror("execve");
      exit(EXIT_FAILURE);
    }
    else{
      playerPids[i] = pid;
    }
  }

  //Hacer select, escuchar mediante el pipe
  int activePlayers = gameStateSHM->playerQty;
  int selectedPlayer = 0;
  
  while(activePlayers > 0){
    fd_set pipeSet;
    FD_ZERO(&pipeSet);
    int maxFD = -1;

    for(int i = 0; i < config.playerCount; i++){
      int pipeFD = pipePlayerToMaster[i][0];
      if(pipeFD != -1){
        FD_SET(pipeFD, &pipeSet);
        if (pipeFD > maxFD){
          maxFD = pipeFD;
        }
      }
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int ready = select(maxFD + 1, &pipeSet, NULL, NULL, &timeout);

    if(ready < 0){
      perror("select");
      break;
    }

    if(ready == 0){
      //Terminar si no responde por tantos segundos

      //sino continuar
      continue;
    }

    //Reiniciar contador de tiempo

    for(int i = 0; i < gameStateSHM->playerQty; i++){
      int index = (selectedPlayer + i) % gameStateSHM->playerQty;
      int pipeFD = pipePlayerToMaster[index][0];

      if(pipeFD == -1){
        continue;
      }

      if(FD_ISSET(pipeFD,&pipeSet)){
        unsigned char mov;
        int n = read(pipeFD, &mov, 1);
        if(n < 0){
          perror("read");
          printf("Player %d closed their pipe (EOF)\n", index);
          pipePlayerToMaster[index][0] = -1;
          activePlayers--;
        } else if(n == 0){
          printf("Player %d closed their pipe (EOF)\n", index);
          pipePlayerToMaster[index][0] = -1;
          activePlayers--;
        } else {
          //Leyo el movimiento
          beginWrite(gameSyncSHM);
          printf("player %d current pos: X:%d Y:%d\n\n", index+1, gameStateSHM->playerList[index].tableX, gameStateSHM->playerList[index].tableY);
          printf("Jugador %d intento mover: %d\n", index+1, mov);
           bool stateMove = validMove(mov, index, gameStateSHM);
           printf("Resultado de validMove para jugador %d: %d\n", index+1, stateMove);
           if(!stateMove){
             printf("Jugador %d hizo un movimiento inválido: %d\n", index+1, mov);
            
             if (gameStateSHM->playerList[index].isBlocked && pipePlayerToMaster[index][0] != -1) {
              printf("Jugador %d está bloqueado. Cerrando su pipe y disminuyendo jugadores activos.\n", index+1);
              close(pipePlayerToMaster[index][0]);
              pipePlayerToMaster[index][0] = -1;
              activePlayers--;
            }
          
           }
          
          printf("Verificando si jugador %d está bloqueado: %d\n", index, gameStateSHM->playerList[index].isBlocked);
           
           

           endWrite(gameSyncSHM);

           printf("[master] posting A for view\n");

           sem_post(&gameSyncSHM->A);
           sem_wait(&gameSyncSHM->B);           

           printf("Active players: %d\n", activePlayers);
           



          //Aplicar logica de juego
        }
        selectedPlayer = (index + 1) % config.playerCount;
        break;
      }
    }
  }

  //Sleep agregado ya que la SHM se borra antes de que vista.c
  //pueda accederla
  sleep(3);

  //Borrar las memorias compartidas
  if(deleteGameStateSHM(GAME_STATE_SHM_NAME,gameStateSHM) == ERROR_VALUE){
    exit(EXIT_FAILURE);
  }
  //Limpio los pipes
  if(clearPipes(config.playerCount, pipePlayerToMaster) == ERROR_VALUE){
    exit(EXIT_FAILURE);
  }
  //Esperamos a los hijos para terminar
  for (int i = 0; i < config.playerCount; i++) {
    int status;
    waitpid(playerPids[i], &status, 0);
  }

  

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

    size_t totalSize = sizeof(gameState) + sizeof(int) * gameState->tableWidth * gameState->tableHeight;
    if(munmap(gameState, sizeof(totalSize)) == ERROR_VALUE) { 
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

void * createGameSyncSHM(char * name) {
    shm_unlink(name);
       
    int fd;
    fd = shm_open(name, O_CREAT | O_RDWR | O_TRUNC, 0644); // ver este permiso
    
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

    if(sem_init(&(gameSync->A), 1, 0) == ERROR_VALUE) {
        perror("Fallo crear el semaforo A\n");
        return NULL;
    }

    if(sem_init(&(gameSync->B), 1, 0) == ERROR_VALUE) {
      perror("Fallo crear el semaforo B\n");
      return NULL;
    }

    if(sem_init(&(gameSync->C), 1, 1) == ERROR_VALUE) {
      perror("Fallo crear el semaforo C\n");
      return NULL;
    }

    if(sem_init(&(gameSync->D), 1, 1) == ERROR_VALUE) {
      perror("Fallo crear el semaforo D\n");
      return NULL;
    }

    if(sem_init(&(gameSync->E), 1, 1) == ERROR_VALUE) {
      perror("Fallo crear el semaforo E\n");
      return NULL;  
    }

    gameSync->F = 0;

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

int clearPipes(int playerCount, int (*pipePlayerToMaster)[2]){
  for (int i = 0; i < playerCount; i++){
    //Limpio segundo pipe de player a master
      if(close(pipePlayerToMaster[i][READ]) == ERROR_VALUE){
      perror("Fallo limpiar el pipe de player a master");
      return ERROR_VALUE;
    }
  }
  return 0;
}

bool validMove(unsigned char mov, int indexPlayer, gameStateSHMStruct * gameStateSHM) {
  //Si es borde, retornar -1 (se quiere mover para afuera)
  if(indexPlayer < 0 || indexPlayer >= 9){
    perror("Indice invalido para acceder a jugador");
    return ERROR_VALUE;
  }

  bool moveOk = checkMovement(indexPlayer, gameStateSHM, mov);
  printf("checkMovement → %d\n", moveOk);

  if (!moveOk) {
    bool hasMoves = checkMoveAvailability(indexPlayer, gameStateSHM);
    printf("checkMoveAvailability → %d\n", hasMoves);

    if (!hasMoves) {
        gameStateSHM->playerList[indexPlayer].isBlocked = true;
        printf("Jugador %d marcado como bloqueado\n", indexPlayer);
    }

    gameStateSHM->playerList[indexPlayer].invalidMoveQty++;
    return false;

  }


  // if(!checkMovement(indexPlayer, gameStateSHM, mov)){
  //   if(!checkMoveAvailability(indexPlayer, gameStateSHM)){
  //     //player no tiene mas movimientos posibles
  //     gameStateSHM->playerList[indexPlayer].isBlocked = true;
  //     printf("Jugador %d marcado como bloqueado\n", indexPlayer);
  //   }
  //   gameStateSHM->playerList[indexPlayer].invalidMoveQty++;
  //   return false;
  // }

  gameStateSHM->playerList[indexPlayer].validMoveQty++;
  
  return true;
}

bool isBorder(int indexPlayer, gameStateSHMStruct * gameStateSHM){
  return (gameStateSHM->playerList[indexPlayer].tableX == 0 || gameStateSHM->playerList[indexPlayer].tableX == (gameStateSHM->tableWidth - 1) || 
  gameStateSHM->playerList[indexPlayer].tableY == 0 || gameStateSHM->playerList[indexPlayer].tableY == (gameStateSHM->tableHeight - 1));
}


bool checkMovement(int indexPlayer, gameStateSHMStruct * gameStateSHM, unsigned char mov){
  // Check if the player is on the border
  if (isBorder(indexPlayer, gameStateSHM)) {
        
    // Player is in the upper-left corner
    if (gameStateSHM->playerList[indexPlayer].tableX == 0 && gameStateSHM->playerList[indexPlayer].tableY == 0) {
        switch (mov) {
            case 0:
            case 1:
            case 5:
            case 6:
            case 7:
                return false;
        }
    }
    
    // Player is in the upper-right corner
    else if (gameStateSHM->playerList[indexPlayer].tableX == gameStateSHM->tableWidth - 1 && gameStateSHM->playerList[indexPlayer].tableY == 0) {
        switch (mov) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 7:
                return false;
        }
    }
    
    // Player is in the lower-left corner
    else if (gameStateSHM->playerList[indexPlayer].tableX == 0 && gameStateSHM->playerList[indexPlayer].tableY == gameStateSHM->tableHeight - 1) {
    
        switch (mov) {
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                return false;
        }
    }
    
    // Player is in the lower-right corner
    else if (gameStateSHM->playerList[indexPlayer].tableX == gameStateSHM->tableWidth - 1 && gameStateSHM->playerList[indexPlayer].tableY == gameStateSHM->tableHeight - 1) {
        
        switch (mov) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
                return false;
        }
    }
    
    // Player is on the border but not in any corner (left column, right column, top row, bottom row)
    else {
        // For example, if the player is on the left column (excluding corners)
        if (gameStateSHM->playerList[indexPlayer].tableX == 0) {
            switch (mov) {
                case 6:
                case 5:
                case 7:
                    return false; // Invalid movement
            }
        }

        // If the player is on the right column (excluding corners)
        else if (gameStateSHM->playerList[indexPlayer].tableX == gameStateSHM->tableWidth - 1) {
            switch (mov) {
                case 2: 
                case 1:
                case 3:
                    return false; // Invalid movement
            }
        }

        // If the player is in the first row (excluding corners)
        else if (gameStateSHM->playerList[indexPlayer].tableY == 0) {
            switch (mov) {
                case 7:
                case 0:
                case 1:
                    return false; // Invalid movement
            }
        }

        // If the player is in the last row (excluding corners)
        else if (gameStateSHM->playerList[indexPlayer].tableY == gameStateSHM->tableHeight - 1) {
            switch (mov) {
                case 3:
                case 4: 
                case 5:
                    return false; // Invalid movement
            }
        }
      }
    }

  int posY = gameStateSHM->playerList[indexPlayer].tableY;
  int posX = gameStateSHM->playerList[indexPlayer].tableX;

  movCases(mov, &posX, &posY);

  //checkea si el casillero ya esta ocupado

  int playerPosInTable = (posY * gameStateSHM->tableWidth) + posX; 

  if(!checkIfOccupied(playerPosInTable, gameStateSHM)){
    int reward = gameStateSHM->tableStartPointer[playerPosInTable];
    gameStateSHM->playerList[indexPlayer].score += reward;
    gameStateSHM->tableStartPointer[playerPosInTable] = (indexPlayer + 1) * (-1);
    gameStateSHM->playerList[indexPlayer].tableX = posX;
    gameStateSHM->playerList[indexPlayer].tableY = posY;
    return true;
  }
  
  return false;
}

bool checkIfOccupied(int posTable, gameStateSHMStruct * gameStateSHM){
  return gameStateSHM->tableStartPointer[posTable] < 1;  
}


bool checkPosAvailability(int iniMov, int limit, gameStateSHMStruct * gameStateSHM, int indexPlayer){

  int posY, posX;  
  int cantInvalid = 0;
  int movIniValid = iniMov;
  int playerPosInTable;

  while(cantInvalid < limit){
    posY = gameStateSHM->playerList[indexPlayer].tableY;
    posX = gameStateSHM->playerList[indexPlayer].tableX;

    movCases((movIniValid++) % 8, &posX, &posY);

    if (posX >= 0 && posX < gameStateSHM->tableWidth && posY >= 0 && posY < gameStateSHM->tableHeight){
      playerPosInTable = (posY * gameStateSHM->tableWidth) + posX;
     if(!checkIfOccupied(playerPosInTable, gameStateSHM)){
       return true;
     }
    cantInvalid++;
    }       
    
  }

  return false;  
}

// if(posX >= 0 && posX < gameStateSHM->tableWidth && posY >= 0 && posY < gameStateSHM->tableHeight){
//   playerPosInTable = (posY * gameStateSHM->tableWidth) + posX;
//   if(!checkIfOccupied(playerPosInTable, gameStateSHM)){
//    return true;
//  }
// cantInvalid++;
// }

void movCases(int mov, int * posX, int * posY) {
  switch (mov) {
    case 0: {
      (*posY)--;
      break;
    }
    case 1: {
      (*posX)++;
      (*posY)--;
      break;
    }
    case 2: {
      (*posX)++;
      break;
    }
    case 3: {
      (*posX)++;
      (*posY)++;
      break;
    }
    case 4: {
      (*posY)++;
      break;
    }
    case 5: {
      (*posX)--;
      (*posY)++;
      break;
    }
    case 6: {
      (*posX)--;
      break;
    }
    case 7: {
      (*posX)--;
      (*posY)--;
      break;
    }
  }
}


bool checkMoveAvailability(int indexPlayer, gameStateSHMStruct * gameStateSHM){

  if(isBorder(indexPlayer, gameStateSHM)){

    // Player is in the upper-left corner
    if (gameStateSHM->playerList[indexPlayer].tableX == 0 && gameStateSHM->playerList[indexPlayer].tableY == 0) {
      return checkPosAvailability(2, LIMITMOVECORNER, gameStateSHM, indexPlayer);       
    }

      // Player is in the upper-right corner
    else if (gameStateSHM->playerList[indexPlayer].tableX == gameStateSHM->tableWidth - 1 && gameStateSHM->playerList[indexPlayer].tableY == 0) {
      return checkPosAvailability(4, LIMITMOVECORNER, gameStateSHM, indexPlayer);  
    }
  
    // Player is in the lower-left corner
    else if (gameStateSHM->playerList[indexPlayer].tableX == 0 && gameStateSHM->playerList[indexPlayer].tableY == gameStateSHM->tableHeight - 1) {
       return checkPosAvailability(0, LIMITMOVECORNER, gameStateSHM, indexPlayer);
    }
  
    // Player is in the lower-right corner
    else if (gameStateSHM->playerList[indexPlayer].tableX == gameStateSHM->tableWidth - 1 && gameStateSHM->playerList[indexPlayer].tableY == gameStateSHM->tableHeight - 1) {
      return checkPosAvailability(6, LIMITMOVECORNER, gameStateSHM, indexPlayer);   
    }
  }
  
  
  // Player is on the border but not in any corner (left column, right column, top row, bottom row)
  else {
      // For example, if the player is on the left column (excluding corners)
      if (gameStateSHM->playerList[indexPlayer].tableX == 0) {
        return checkPosAvailability(0, LIMITMOVEBORDER, gameStateSHM, indexPlayer);
      }

      // If the player is on the right column (excluding corners)
      else if (gameStateSHM->playerList[indexPlayer].tableX == gameStateSHM->tableWidth - 1) {
        return checkPosAvailability(4, LIMITMOVEBORDER, gameStateSHM, indexPlayer);
      }

      // If the player is in the first row (excluding corners)
      else if (gameStateSHM->playerList[indexPlayer].tableY == 0) {
        return checkPosAvailability(2, LIMITMOVEBORDER, gameStateSHM, indexPlayer);
      }

      // If the player is in the last row (excluding corners)
      else if (gameStateSHM->playerList[indexPlayer].tableY == gameStateSHM->tableHeight - 1) {
        return checkPosAvailability(6, LIMITMOVEBORDER, gameStateSHM, indexPlayer);
      }
  }

  //player is on the interior

  return checkPosAvailability(0, LIMITMOVEINTERIOR, gameStateSHM, indexPlayer);
}

// validMovements = {7 = arriba izquierda, 0 = arriba, 1 = arriba derecha}
//                   6 = izquierda,                  , 2 = derecha       }
//                   5 = abajo izquierda,  4 = abajo , 3 = abajo derecha }

void beginWrite(gameSyncSHMStruct * gameSyncSHM) {
  sem_wait(&gameSyncSHM->D);  // Acquire exclusive access to the state
}

void endWrite(gameSyncSHMStruct * gameSyncSHM) {
  sem_post(&gameSyncSHM->D);  // Release exclusive access
}



