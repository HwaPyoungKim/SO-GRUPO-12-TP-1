#include "utils.h"

#define LIMITMOVEINTERIOR 8
#define LIMITMOVEBORDER 5
#define LIMITMOVECORNER 3

#define ERROR_VALUE -1

static void movCases(int mov, int * posX, int * posY) {
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

static bool checkIfOccupied(int posTable, gameStateSHMStruct * gameStateSHM){
  return gameStateSHM->tableStartPointer[posTable] < 1;  
}

static bool isMoveLegal(int x, int y, gameStateSHMStruct *gameState) {
  if (x < 0 || y < 0 || x >= gameState->tableWidth || y >= gameState->tableHeight)
      return false;
  
  int idx = y * gameState->tableWidth + x;
  return !checkIfOccupied(idx, gameState); 
}

static bool checkPosAvailability(int iniMov, int limit, gameStateSHMStruct * gameStateSHM, int indexPlayer){

  int posY, posX;  
  int cantInvalid = 0;
  int movIniValid = iniMov;

  while(cantInvalid < limit){
    posY = gameStateSHM->playerList[indexPlayer].tableY;
    posX = gameStateSHM->playerList[indexPlayer].tableX;

    movCases((movIniValid++) % 8, &posX, &posY);

    if (isMoveLegal(posX, posY, gameStateSHM)) {
      return true;
    }
    cantInvalid++;
  }       
  return false;  
}

static bool isBorder(int indexPlayer, gameStateSHMStruct * gameStateSHM){
  return (gameStateSHM->playerList[indexPlayer].tableX == 0 || gameStateSHM->playerList[indexPlayer].tableX == (gameStateSHM->tableWidth - 1) || 
  gameStateSHM->playerList[indexPlayer].tableY == 0 || gameStateSHM->playerList[indexPlayer].tableY == (gameStateSHM->tableHeight - 1));
}


static bool checkMovement(int indexPlayer, gameStateSHMStruct * gameStateSHM, unsigned char mov){

  int posY = gameStateSHM->playerList[indexPlayer].tableY;
  int posX = gameStateSHM->playerList[indexPlayer].tableX;
  movCases(mov, &posX, &posY);

  if (!isMoveLegal(posX, posY, gameStateSHM)){
    return false;
  }

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

static bool checkMoveAvailability(int indexPlayer, gameStateSHMStruct * gameStateSHM){

  if(isBorder(indexPlayer, gameStateSHM)){

    // Check corners
    if (gameStateSHM->playerList[indexPlayer].tableX == 0 && gameStateSHM->playerList[indexPlayer].tableY == 0) {
      return checkPosAvailability(2, LIMITMOVECORNER, gameStateSHM, indexPlayer);       
    }

    else if (gameStateSHM->playerList[indexPlayer].tableX == gameStateSHM->tableWidth - 1 && gameStateSHM->playerList[indexPlayer].tableY == 0) {
      return checkPosAvailability(4, LIMITMOVECORNER, gameStateSHM, indexPlayer);  
    }
  
    else if (gameStateSHM->playerList[indexPlayer].tableX == 0 && gameStateSHM->playerList[indexPlayer].tableY == gameStateSHM->tableHeight - 1) {
       return checkPosAvailability(0, LIMITMOVECORNER, gameStateSHM, indexPlayer);
    }
  
    else if (gameStateSHM->playerList[indexPlayer].tableX == gameStateSHM->tableWidth - 1 && gameStateSHM->playerList[indexPlayer].tableY == gameStateSHM->tableHeight - 1) {
      return checkPosAvailability(6, LIMITMOVECORNER, gameStateSHM, indexPlayer);   
    }
  }
  
  else {
      // Check borders
      if (gameStateSHM->playerList[indexPlayer].tableX == 0) {
        return checkPosAvailability(0, LIMITMOVEBORDER, gameStateSHM, indexPlayer);
      }

      else if (gameStateSHM->playerList[indexPlayer].tableX == gameStateSHM->tableWidth - 1) {
        return checkPosAvailability(4, LIMITMOVEBORDER, gameStateSHM, indexPlayer);
      }

      else if (gameStateSHM->playerList[indexPlayer].tableY == 0) {
        return checkPosAvailability(2, LIMITMOVEBORDER, gameStateSHM, indexPlayer);
      }

      else if (gameStateSHM->playerList[indexPlayer].tableY == gameStateSHM->tableHeight - 1) {
        return checkPosAvailability(6, LIMITMOVEBORDER, gameStateSHM, indexPlayer);
      }
  }

  // Player is not in corners and borders

  return checkPosAvailability(0, LIMITMOVEINTERIOR, gameStateSHM, indexPlayer);
}

void createPlayer(gameStateSHMStruct * gameStateSHM, int playerIndex, int columns, int cellWidth, int cellHeight, char * playerName, pid_t pid){
  playerSHMStruct * player = &gameStateSHM->playerList[playerIndex];
    strncpy(player->playerName, playerName, sizeof(player->playerName) - 1);
    player->playerName[sizeof(player->playerName) - 1] = '\0';

    int row = playerIndex / columns;
    int col = playerIndex % columns;

    int xIni = col * cellWidth;
    int yIni = row * cellHeight;

    int x = xIni + cellWidth / 2;
    int y = yIni + cellHeight / 2;

    unsigned short width = gameStateSHM->tableWidth;
    unsigned short height = gameStateSHM->tableHeight; 

    if (x >= width) x = width - 1;
    if (y >= height) y = height - 1;

    player->score = 0;
    player->invalidMoveQty = 0;
    player->validMoveQty = 0;
    player->tableX = x;
    player->tableY = y; 
    player->isBlocked = false;
    player->playerPID = pid;

    gameStateSHM->tableStartPointer[y * width + x] = (playerIndex + 1) * (-1); 
  return;
}

// Verify valid move
bool validAndApplyMove(unsigned char mov, int indexPlayer, gameStateSHMStruct * gameStateSHM){
    //If is border, return -1 (player wants to move outside the table)
    if(indexPlayer < 0 || indexPlayer >= 9){
      perror("Indice invalido para acceder a jugador\n");
      return ERROR_VALUE;
    }
  
    bool moveOk = checkMovement(indexPlayer, gameStateSHM, mov);
  
    if (!moveOk) {
      bool hasMoves = checkMoveAvailability(indexPlayer, gameStateSHM);
      if (!hasMoves) {
          gameStateSHM->playerList[indexPlayer].isBlocked = true;
      }
      gameStateSHM->playerList[indexPlayer].invalidMoveQty++;
      return false;
    }
  
    gameStateSHM->playerList[indexPlayer].validMoveQty++;
    return true;
}

int findWinner(gameStateSHMStruct * gameStateSHM){
  int limit = gameStateSHM->playerQty;
  int winnerIndex = -1;
  int scoreAux = 0;
  int validMoveAux = 0;
  int invalidMoveAux = 0;
  int currentScore, currentValidMove, currentInvalidMove;

  for(int i = 0; i < limit; i++){
    currentScore = gameStateSHM->playerList[i].score;
    currentValidMove = gameStateSHM->playerList[i].validMoveQty;
    currentInvalidMove = gameStateSHM->playerList[i].invalidMoveQty;

    if(currentScore > scoreAux || (currentScore == scoreAux && currentValidMove < validMoveAux) || (currentScore == scoreAux && currentValidMove == validMoveAux && currentInvalidMove < invalidMoveAux)){
      winnerIndex = i;
      scoreAux = currentScore;
      validMoveAux = currentValidMove;
      invalidMoveAux = currentInvalidMove;
    }    
  }
  return winnerIndex;
}

void printPlayer(int playerIndex, int status,gameStateSHMStruct * gameStateSHM){
  printf("Player %s (%d) exited (%d) with score of %d / %d / %d\n", gameStateSHM->playerList[playerIndex].playerName, playerIndex, status, gameStateSHM->playerList[playerIndex].score,gameStateSHM->playerList[playerIndex].validMoveQty, gameStateSHM->playerList[playerIndex].invalidMoveQty);
  return;
}

void printWinner(int playerIndex, gameStateSHMStruct * gameStateSHM){
  printf("\n######### Winner is %s #########\n\n", gameStateSHM->playerList[playerIndex].playerName);
}


