#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include "sharedMemory.h"
#include <sys/mman.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>

#define GAME_STATE_SHM_NAME "/game_state"
#define GAME_SYNC_SHM_NAME "/game_sync"

void endRead(gameSyncSHMStruct *sync);
void beginRead(gameSyncSHMStruct *sync);

int main(int argc, char *argv[]){
  //recibe como parametro el ancho y alto del tablero

  if (argc != 3) {
    fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
    return 1;
  }

  int width = atoi(argv[1]);
  int height = atoi(argv[2]);

  //Se conecta con ambas memorias compartidas

  
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


  gameStateSHMStruct *gameStateSHM = mmap(NULL, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, gameStateFD, 0);
  if (gameStateSHM == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  gameSyncSHMStruct *gameSyncSHM = mmap(NULL, sizeof(gameSyncSHMStruct), PROT_READ | PROT_WRITE, MAP_SHARED, gameSyncFD, 0);
  if (gameSyncSHM == MAP_FAILED) {
    perror("mmap GAME_SYNC");
    exit(EXIT_FAILURE);
  }



  //Mientras no esté bloqueado, consultar el estado del tablero de forma sincronizada entre el
  //máster y el resto de jugadores y enviar solicitudes de movimientos al máster.

  //Ejemplo

    srand(time(NULL) ^ getpid());

    // beginRead(gameSyncSHM);
    // endRead(gameSyncSHM);

    // Simulamos el envío de 20 movimientos
    // for (int i = 0; i < 20; i++) {
    //     unsigned char movimiento = rand() % 8; // [0-7], direcciones válidas
    //     write(1, &movimiento, sizeof(movimiento)); // escribir al stdout → máster lo recibe por el pipe
    //     usleep(3000);  // Espera 300 ms para no spamear
    // }

    while(1){
        unsigned char movimiento = rand() % 8;
        write(1, &movimiento, sizeof(movimiento));
    }

    // unsigned char movimiento = rand() % 8;
    // write(1, &movimiento, sizeof(movimiento));

    close(gameStateFD);
    close(gameSyncFD);


    return 0;
}



///////////////////////////////////////////////////////////////////////////////////////////


void beginRead(gameSyncSHMStruct *sync) {
    sem_wait(&sync->C);
    sem_wait(&sync->E);
    sync->F++;
    if (sync->F == 1) sem_wait(&sync->D);
    sem_post(&sync->E);
    sem_post(&sync->C);
}

void endRead(gameSyncSHMStruct *sync) {
    sem_wait(&sync->E);
    sync->F--;
    if (sync->F == 0) sem_post(&sync->D);
    sem_post(&sync->E);
}


