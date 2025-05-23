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

#define WINDOW_HEIGHT 200
#define WINDOW_LENGHT 200

void endRead(gameSyncSHMStruct * sync);
void beginRead(gameSyncSHMStruct * sync);
void cleanUp(WINDOW * offscreen);

void drawBoard(gameStateSHMStruct *gameState, WINDOW * offscreen);
void printTablero(int * table, int width, int height);

int main(int argc, char *argv[]){ 

  setenv("TERM", "xterm", 1); 
  initscr();
  noecho();
  curs_set(FALSE);

  sleep(1);
  if (argc < 3) {
    fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
    return 1;
  }

  int width = atoi(argv[1]);
  int height = atoi(argv[2]);

  size_t totalSize = sizeof(gameStateSHMStruct) + sizeof(int) * width * height;

  int gameStateFD = shm_open(GAME_STATE_SHM_NAME, O_RDONLY, 0666);
  if (gameStateFD == -1) {
    perror("shm_open GAME_STATE");
    exit(EXIT_FAILURE);
  }

  int gameSyncFD = shm_open(GAME_SYNC_SHM_NAME, O_RDWR, 0666);
  if (gameSyncFD == -1) {
    perror("shm_open GAME_SYNC");
    exit(EXIT_FAILURE);
  }

  gameStateSHMStruct * gameStateSHM = mmap(NULL, totalSize, PROT_READ , MAP_SHARED, gameStateFD, 0);
  if (gameStateSHM == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  gameSyncSHMStruct * gameSyncSHM = mmap(NULL, sizeof(gameSyncSHMStruct), PROT_READ | PROT_WRITE, MAP_SHARED, gameSyncFD, 0);
  if (gameSyncSHM == MAP_FAILED) {
    perror("mmap GAME_SYNC");
    exit(EXIT_FAILURE);
  }

  WINDOW *offscreen = newwin(WINDOW_HEIGHT, WINDOW_LENGHT, 0, 0);

  bool flag = true;
  while(flag){
    sem_wait(&gameSyncSHM->semPendingView); 

    if(!gameStateSHM->gameState){
      flag = false;
      cleanUp(offscreen);
    } else {
      drawBoard(gameStateSHM, offscreen);
    }
    
    sem_post(&gameSyncSHM->semFinishedView); 
  }
  munmap(gameStateSHM, totalSize);
  munmap(gameSyncSHM, sizeof(gameSyncSHMStruct));

  close(gameStateFD);
  close(gameSyncFD);

  cleanUp(offscreen); 
  
  return 0;
}

void cleanUp(WINDOW * offscreen){
  delwin(offscreen);
  werase(stdscr);
  wrefresh(stdscr);
  endwin();
}

void drawBoard(gameStateSHMStruct *gameState, WINDOW * offscreen) {
  
  werase(offscreen);
  start_color();

  init_pair(1, COLOR_RED, COLOR_BLACK);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(3, COLOR_YELLOW, COLOR_BLACK);
  init_pair(4, COLOR_BLUE, COLOR_BLACK);
  init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(6, COLOR_CYAN, COLOR_BLACK);
  init_pair(7, COLOR_BLACK, COLOR_GREEN); 
  init_pair(8, COLOR_BLACK, COLOR_YELLOW);
  init_pair(9, COLOR_BLACK, COLOR_CYAN);
  init_pair(10, COLOR_WHITE, COLOR_BLACK); 


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
              wattron(offscreen, COLOR_PAIR(playerIndex + 1));
              mvwprintw(offscreen, y, x * 4, "P%d", playerIndex + 1);
              wattroff(offscreen, COLOR_PAIR(playerIndex + 1));
          } else if (cell < 0) {
              int jugadorID = -cell;
              wattron(offscreen, COLOR_PAIR(10));
              mvwprintw(offscreen, y, x * 4, "P%d", jugadorID);
              wattroff(offscreen, COLOR_PAIR(10));
          } else if (cell > 0) {
              wattron(offscreen, COLOR_PAIR(10));
              mvwprintw(offscreen, y, x * 4, " %d ", cell);
              wattroff(offscreen, COLOR_PAIR(10));
          }
      }
  }

  int baseY = gameState->tableHeight + 1;
  wattron(offscreen, COLOR_PAIR(11));
  mvwprintw(offscreen, baseY, 0, "PUNTAJES:");
  wattroff(offscreen, COLOR_PAIR(11));

  for (int i = 0; i < gameState->playerQty; i++) {
      wattron(offscreen, COLOR_PAIR(i + 1));
      mvwprintw(offscreen, baseY + i + 1, 0, "Jugador %d(%s): %d pts | movs validos: %d | movs invalidos: %d", i+1, gameState->playerList[i].playerName, gameState->playerList[i].score, gameState->playerList[i].validMoveQty, gameState->playerList[i].invalidMoveQty);
      wattroff(offscreen, COLOR_PAIR(i + 1));
  }
  wnoutrefresh(offscreen);
  doupdate();
  
}