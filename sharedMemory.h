#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <semaphore.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>

typedef struct {
  char playerName[16]; 
  unsigned int score; 
  unsigned int invalidMoveQty; 
  unsigned int validMoveQty; 
  unsigned short tableX, tableY; 
  pid_t playerPID;
  bool isBlocked; 
} playerSHMStruct;

typedef struct {
  unsigned short tableWidth; 
  unsigned short tableHeight; 
  unsigned int playerQty; 
  playerSHMStruct playerList[9]; 
  bool gameState; 
  int tableStartPointer[]; 
} gameStateSHMStruct;

typedef struct {
  sem_t semPendingView; 
  sem_t semFinishedView; 

  sem_t writerPrivilege;
  sem_t masterPlayerMutex; 
  sem_t playersReadingCountMutex; 
  unsigned int playersReadingCount; 
} gameSyncSHMStruct;

