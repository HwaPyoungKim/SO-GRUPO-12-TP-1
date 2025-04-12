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

#define GAME_STATE_SHM_NAME "/game_state"
#define GAME_SYNC_SHM_NAME "/game_sync"

void endRead(gameSyncSHMStruct * sync);
void beginRead(gameSyncSHMStruct * sync);

void printTablero(int * table, int width, int height);

int main(int argc, char *argv[]){ 
  //Sleep para que le de tiempo al master de crear la SHM
  sleep(1);
  if (argc < 3) {
    fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
    return 1;
  }

  int width = atoi(argv[1]);
  int height = atoi(argv[2]);

  size_t totalSize = sizeof(gameStateSHMStruct) + sizeof(int) * width * height;

  int gameStateFD = shm_open(GAME_STATE_SHM_NAME, O_RDWR, 0666);
  if (gameStateFD == -1) {
    perror("shm_open xd");
    exit(EXIT_FAILURE);
  }

  int gameSyncFD = shm_open(GAME_SYNC_SHM_NAME, O_RDWR, 0666);
  if (gameSyncFD == -1) {
    perror("shm_open GAME_SYNC");
    exit(EXIT_FAILURE);
  }

  gameStateSHMStruct * gameStateSHM = mmap(NULL, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, gameStateFD, 0);
  if (gameStateSHM == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  gameSyncSHMStruct * gameSyncSHM = mmap(NULL, sizeof(gameSyncSHMStruct), PROT_READ | PROT_WRITE, MAP_SHARED, gameSyncFD, 0);
  if (gameSyncSHM == MAP_FAILED) {
    perror("mmap GAME_SYNC");
    exit(EXIT_FAILURE);
  }

  bool flag = true;
  while(flag){
    printf("[view] waiting on A...\n");
    sem_wait(&gameSyncSHM->A); // wait for master to signal

    if(!gameStateSHM->gameState){
      printf("Entro al flag\n");
      flag = false;
    } else {
      printf("[view] passed A\n");
      printTablero(gameStateSHM->tableStartPointer, gameStateSHM->tableWidth, gameStateSHM->tableHeight);
    }
    sem_post(&gameSyncSHM->B); // tell master we’re done printing
  }
  
  printf("Termino vista\n");

  close(gameStateFD);
  close(gameSyncFD);
  
  return 0;
}

void printTablero(int * table, int width, int height) {
  printf("\nEstado del tablero:\n\n");

  for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
          int index = y * width + x;
          int cell = table[index];

          if (cell > 0) {
              printf(" %d  ", cell); // recompensa (1-9)
          } else {
              printf(" P%d ", -cell); // jugador (almacenado como -1, -2, etc.)
          }
      }
      printf("\n\n");
  }

  printf("\n");
}


/////////////////////////////////////
