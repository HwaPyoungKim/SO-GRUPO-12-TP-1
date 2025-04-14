#include "utils.h"


#define DEFAULT_HEIGHT 10
#define DEFAULT_WIDTH 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 10
#define GAME_STATE_SHM_NAME "/game_state"
#define GAME_SYNC_SHM_NAME "/game_sync"

#define MILISECS_TO_SECS 1000
#define MICROSECS_TO_MILISECS 1000

#define MAX_PLAYERS 9

#define WRITE 0
#define READ 1


void * createGameStateSHM(char * name, config_t * config);
void * createGameSyncSHM(char * name);
int deleteGameStateSHM(char * name, gameStateSHMStruct * gameState);
int deleteGameSyncSHM(char * name, gameSyncSHMStruct * gameSync);
int clearPipes(int playerCount, int (*pipePlayerToMaster)[2]);

void beginWrite(gameSyncSHMStruct * gameSyncSHM);
void endWrite(gameSyncSHMStruct * gameSyncSHM);
void printPlayer(int playerIndex, int status,gameStateSHMStruct * gameStateSHM);

int
main(int argc, char * argv[]) {

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

  printf("width: %d\n", config.width);
  printf("height: %d\n", config.height);
  printf("delay: %d\n", config.delay);
  printf("timeout: %d\n", config.timeout);
  printf("seed: %d\n", config.seed);
  printf("view: %s\n", config.view_path == NULL ? "-" : config.view_path);
  printf("num_players: %d\n", config.playerCount);
  for (int i = 0; i < config.playerCount; i++){
    printf("  %s\n", config.playerPaths[i]);
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

  pid_t viewPID;

  if(config.view_path != NULL){
    pid_t pid = fork();
    if (pid == ERROR_VALUE) {
      perror("fork");
      exit(EXIT_FAILURE);
    }

    if(pid == 0){
      char width_str[10], height_str[10];
      snprintf(width_str, sizeof(width_str), "%d", config.width);
      snprintf(height_str, sizeof(height_str), "%d", config.height);

      char * arguments[] = { config.view_path, width_str, height_str, NULL };

      execve(config.view_path, arguments, NULL);

      perror("execve");
      exit(EXIT_FAILURE);
    }else{
      viewPID = pid;
    }
  }

  int pipePlayerToMaster[config.playerCount][2];
  pid_t playerPids[config.playerCount];

  int width = gameStateSHM->tableWidth;
  int height = gameStateSHM->tableHeight;
  int playerQty = gameStateSHM->playerQty;

  int rows = (int) sqrt(playerQty);
  int columns = (playerQty + rows - 1) / rows;

  int cellWidth = width / columns;
  int cellHeight = height / rows;

  for (int i = 0; i < config.playerCount; i++) {

    createPlayer(gameStateSHM, i, columns, cellWidth, cellHeight, config.playerPaths[i]);

    if (pipe(pipePlayerToMaster[i]) == ERROR_VALUE) {
      perror("Error in pipe player to master");
      exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == ERROR_VALUE) {
      perror("fork");
      exit(EXIT_FAILURE);
    }
    
    if(pid == 0){
      close(pipePlayerToMaster[i][0]); // Cierra lectura
      dup2(pipePlayerToMaster[i][1], STDOUT_FILENO); // Redirige stdout al pipe

      char width_str[10], height_str[10];
      snprintf(width_str, sizeof(width_str), "%d", config.width);
      snprintf(height_str, sizeof(height_str), "%d", config.height);

      char *arguments[] = { config.playerPaths[i], width_str, height_str, NULL };

      execve(config.playerPaths[i], arguments, NULL);

      perror("execve");
      gameStateSHM->playerList[i].isBlocked = true;
      exit(EXIT_FAILURE);
    } else {
      playerPids[i] = pid;
    }
  
  }

  int activePlayers = gameStateSHM->playerQty;
  int selectedPlayer = 0;
  size_t validMoveInterval = 0;

  while(activePlayers > 0 && gameStateSHM->gameState){

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
      continue;
    }

    for(int i = 0; i < gameStateSHM->playerQty && gameStateSHM->gameState; i++){
      int index = (selectedPlayer + i) % gameStateSHM->playerQty;
      int pipeFD = pipePlayerToMaster[index][0];

      if(pipeFD == -1){
        continue;
      }

      if(FD_ISSET(pipeFD,&pipeSet)){
        unsigned char mov;
        int n = read(pipeFD, &mov, 1);
        if(n < 0){
          perror("fallo en read");
        } else if(n == 0){
          pipePlayerToMaster[index][0] = -1;
          activePlayers--;
        } else {
          //Leyo el movimiento

          if(validMoveInterval >= (config.timeout * MILISECS_TO_SECS)){
            beginWrite(gameSyncSHM);
            for(int i = 0; i < gameStateSHM->playerQty; i++){
              gameStateSHM->playerList[i].isBlocked = true;
              close(pipePlayerToMaster[i][0]);
              pipePlayerToMaster[i][0] = -1;
              activePlayers--;
            }
            gameStateSHM->gameState = false;

            endWrite(gameSyncSHM);

            usleep(config.delay * MICROSECS_TO_MILISECS);

          } else {
            beginWrite(gameSyncSHM);
            bool stateMove = validAndApplyMove(mov, index, gameStateSHM);
            if(!stateMove){
              
              if (gameStateSHM->playerList[index].isBlocked && pipePlayerToMaster[index][0] != -1) {
                
                close(pipePlayerToMaster[index][0]);
                pipePlayerToMaster[index][0] = -1;
                activePlayers--;
              }
            } else {
              validMoveInterval = 0;
            }
            
            endWrite(gameSyncSHM);
  
            usleep(config.delay * MICROSECS_TO_MILISECS);
            validMoveInterval += (size_t)config.delay;
  
            if(config.view_path != NULL){
              sem_post(&gameSyncSHM->A);
              sem_wait(&gameSyncSHM->B);   
            } 
          }    
        }
        selectedPlayer = (index + 1) % config.playerCount;
        break;
      }
    }
  }

  gameStateSHM->gameState = false;
  if(config.view_path != NULL){
    sem_post(&gameSyncSHM->A);
    sem_wait(&gameSyncSHM->B);  
  }

  int statusArray[MAX_PLAYERS] = {0};

  for (int i = 0; i < config.playerCount; i++) {
    int status;
    waitpid(playerPids[i], &status, 0);
    statusArray[i] = status; 
  }

  int viewStatus;
  if(config.view_path != NULL){
    waitpid(viewPID, &viewStatus, 0);
  }

  for (int i = 0; i < config.playerCount; i++) {
    printPlayer(i, WEXITSTATUS(statusArray[i]), gameStateSHM);
  }

  printf("View %s exited (%d)\n", config.view_path, viewStatus);

  if(deleteGameStateSHM(GAME_STATE_SHM_NAME,gameStateSHM) == ERROR_VALUE){
    exit(EXIT_FAILURE);
  }


  if(deleteGameSyncSHM(GAME_SYNC_SHM_NAME, gameSyncSHM) == ERROR_VALUE){
    exit(EXIT_FAILURE);
  }

  
  if(clearPipes(config.playerCount, pipePlayerToMaster) == ERROR_VALUE){
    exit(EXIT_FAILURE);
  }

}

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
    gameState->gameState = true;

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

    if(sem_init(&(gameSync->writerPrivilege), 1, 1) == ERROR_VALUE) {
      perror("Fallo crear el semaforo writerPrivilege\n");
      return NULL;
    }

    if(sem_init(&(gameSync->masterPlayerMutex), 1, 1) == ERROR_VALUE) {
      perror("Fallo crear el semaforo D\n");
      return NULL;
    }

    if(sem_init(&(gameSync->playersReadingCountMutex), 1, 1) == ERROR_VALUE) {
      perror("Fallo crear el semaforo playersReadingCountMutex\n");
      return NULL;  
    }

    gameSync->playersReadingCount = 0;

    if(close(fd) == ERROR_VALUE) {
        perror("Fallo cerrar la memoria compartida del game sync\n");
        return NULL;
    }

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

    if(sem_destroy(&(gameSync->writerPrivilege)) == ERROR_VALUE) {
      perror("Fallo cerrar el semaforo writerPrivilege");
      return ERROR_VALUE;
    }

    if(sem_destroy(&(gameSync->masterPlayerMutex)) == ERROR_VALUE) {
      perror("Fallo cerrar el semaforo D");
      return ERROR_VALUE;
    }

    if(sem_destroy(&(gameSync->playersReadingCountMutex)) == ERROR_VALUE) {
      perror("Fallo cerrar el semaforo playersReadingCountMutex");
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

// validMovements = {7 = arriba izquierda, 0 = arriba, 1 = arriba derecha}
//                   6 = izquierda,                  , 2 = derecha       }
//                   5 = abajo izquierda,  4 = abajo , 3 = abajo derecha } 

void beginWrite(gameSyncSHMStruct * gameSyncSHM) {
  sem_wait(&gameSyncSHM->writerPrivilege);    // Impide que se agreguen lectores
  sem_wait(&gameSyncSHM->masterPlayerMutex);  //Acceso a que corra el master
  sem_post(&gameSyncSHM->writerPrivilege);    //Libera escritura
}

void endWrite(gameSyncSHMStruct * gameSyncSHM) {
  sem_post(&gameSyncSHM->masterPlayerMutex);  // Libera acceso privilegiado
}


