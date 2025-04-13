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
#include <ncurses.h>

#define GAME_STATE_SHM_NAME "/game_state"
#define GAME_SYNC_SHM_NAME "/game_sync"
#define PLAYER_COLOR 1
#define DEFAULT_COLOR 2

void endRead(gameSyncSHMStruct * sync);
void beginRead(gameSyncSHMStruct * sync);

void drawBoard(gameStateSHMStruct *gameState);
void printTablero(int * table, int width, int height);

int main(int argc, char *argv[]){ 

  setenv("TERM", "xterm", 1); 
  initscr();
  noecho();
  curs_set(FALSE);

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
    sem_wait(&gameSyncSHM->A); // wait for master to signal

    if(!gameStateSHM->gameState){
      flag = false;
    } else {
      //printTablero(gameStateSHM->tableStartPointer, gameStateSHM->tableWidth, gameStateSHM->tableHeight);
      drawBoard(gameStateSHM);
    }
    sem_post(&gameSyncSHM->B); // tell master we’re done printing
  }


  close(gameStateFD);
  close(gameSyncFD);

  endwin();
  
  return 0;
}

// void printTablero(int * table, int width, int height) {
//   printf("\nEstado del tablero:\n\n");

//   for (int y = 0; y < height; y++) {
//       for (int x = 0; x < width; x++) {
//           int index = y * width + x;
//           int cell = table[index];

//           if (cell > 0) {
//               printf(" %d  ", cell); // recompensa (1-9)
//           } else {
//               printf(" P%d ", -cell); // jugador (almacenado como -1, -2, etc.)
//           }
//       }
//       printf("\n\n");
//   }

//   printf("\n");
// }

void drawBoard(gameStateSHMStruct *gameState) {
  clear(); 
  start_color();

  for(int i = 0; i < 9; i++) {
      init_pair(i + 1, i + 1, COLOR_BLACK);
  }
  init_pair(10, COLOR_WHITE, COLOR_BLACK);
  init_pair(11, COLOR_WHITE, COLOR_BLACK); 

  int width = gameState->tableWidth;
  int height = gameState->tableHeight;
  int * table = gameState->tableStartPointer;

  for(int y = 0; y < height; y++) {
      for(int x = 0; x < width; x++) {
          int index = y * width + x;
          int cell = table[index];

          bool isLatest = false;
          int playerIndex = -1;

          for(int i = 0; i < gameState->playerQty; i++) {
              if (gameState->playerList[i].tableX == x && gameState->playerList[i].tableY == y) {
                  isLatest = true;
                  playerIndex = i;
                  break;
              }
          }
          if (isLatest) {
              attron(COLOR_PAIR(playerIndex + 1));
              mvprintw(y, x * 4, "P%d", playerIndex + 1);
              attroff(COLOR_PAIR(playerIndex + 1));
          } else if (cell < 0) {
              int jugadorID = -cell;
              attron(COLOR_PAIR(10));
              mvprintw(y, x * 4, "P%d", jugadorID);
              attroff(COLOR_PAIR(10));
          } else if (cell > 0) {
              attron(COLOR_PAIR(10));
              mvprintw(y, x * 4, " %d ", cell);
              attroff(COLOR_PAIR(10));
          }
      }
  }

  int baseY = gameState->tableHeight + 1;
  attron(COLOR_PAIR(11));
  mvprintw(baseY, 0, "PUNTAJES:");
  attroff(COLOR_PAIR(11));

  for (int i = 0; i < gameState->playerQty; i++) {
      attron(COLOR_PAIR(i + 1));
      mvprintw(baseY + i + 1, 0,
               "Jugador %d (%s): %d pts | Movs válidos: %d | Inválidos: %d",
               i + 1, gameState->playerList[i].playerName, gameState->playerList[i].score,
               gameState->playerList[i].validMoveQty, gameState->playerList[i].invalidMoveQty);
      attroff(COLOR_PAIR(i + 1));
  }

  refresh(); 
}
