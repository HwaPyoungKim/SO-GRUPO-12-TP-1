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

// Define colors
#define PLAYER_COLOR 1
#define DEFAULT_COLOR 2

void endRead(gameSyncSHMStruct * sync);
void beginRead(gameSyncSHMStruct * sync);

void drawBoard(gameStateSHMStruct *gameState);
void printTablero(int * table, int width, int height);

int main(int argc, char *argv[]){ 

  // Set the TERM variable to a known terminal type (e.g., xterm)
  setenv("TERM", "xterm", 1);  // Setting TERM to 'xterm'

  // Start ncurses
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

void drawBoard(gameStateSHMStruct *gameState) {
  clear(); // Clears the screen before redrawing the board

  // Initialize color pairs (You can change the color codes as needed)
  start_color();
  init_pair(PLAYER_COLOR, COLOR_RED, COLOR_BLACK);  // Player positions in red
  init_pair(DEFAULT_COLOR, COLOR_WHITE, COLOR_BLACK);  // Default cells in white

  // Loop through each player to highlight their current position
  for (int y = 0; y < gameState->tableHeight; y++) {
    for (int x = 0; x < gameState->tableWidth; x++) {
      int index = y * gameState->tableWidth + x;
      int cellValue = gameState->tableStartPointer[index];

      // Find if this cell is occupied by a player
      bool isPlayerCell = false;
      int i;
      for (i = 0; i < gameState->playerQty; i++) {
        if (gameState->playerList[i].tableX == x && gameState->playerList[i].tableY == y) {
          isPlayerCell = true;
          break;  // Player found, no need to check further players
        }
      }
      if (isPlayerCell) {
        // If it's a player, highlight the position
        attron(COLOR_PAIR(PLAYER_COLOR));
        mvprintw(y, x * 4, " P%d ", i+1);  // Adjust column spacing for better alignment
        attroff(COLOR_PAIR(PLAYER_COLOR));
      } else {
        // Else, print with default color
        attron(COLOR_PAIR(DEFAULT_COLOR));
        if (cellValue > 0) {
          mvprintw(y, x * 4, " %d ", cellValue);  // Print regular cell values
        } else {
          mvprintw(y, x * 4, " . ");  // Print empty cell
        }
        attroff(COLOR_PAIR(DEFAULT_COLOR));
      }
    }
  }
  int base_y = gameState->tableHeight + 1;

  mvprintw(base_y, 0, "PUNTAJES:");

  for (int i = 0; i < gameState->playerQty; i++) {

    mvprintw(base_y + i + 1, 0,
        "Jugador %d (%s): %d pts | Movs válidos: %d | Inválidos: %d",
        i+1, gameState->playerList[i].playerName, gameState->playerList[i].score, gameState->playerList[i].validMoveQty, gameState->playerList[i].invalidMoveQty);
  }

  refresh(); // Update the screen
}