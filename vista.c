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

void printTablero(int *table, int width, int height);

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

  int fd = shm_open(GAME_STATE_SHM_NAME, O_RDWR, 0666);
  if (fd == -1) {
    perror("shm_open xd");
    exit(EXIT_FAILURE);
  }

  gameStateSHMStruct *gameStateSHM = mmap(NULL, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (gameStateSHM == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  printTablero(gameStateSHM->tableStartPointer, gameStateSHM->tableWidth, gameStateSHM->tableHeight);

  close(fd);
}

void printTablero(int *table, int width, int height) {
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