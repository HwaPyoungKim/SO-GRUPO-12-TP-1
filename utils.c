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

  int x = gameStateSHM->playerList[indexPlayer].tableX;
  int y = gameStateSHM->playerList[indexPlayer].tableY;
  movCases(mov, &x, &y);

  if (!isMoveLegal(x, y, gameStateSHM)){
    return false;
  }

  int posY = gameStateSHM->playerList[indexPlayer].tableY;
  int posX = gameStateSHM->playerList[indexPlayer].tableX;

  int newX = posX;
  int newY = posY;

  movCases(mov, &newX, &newY);

  if (newX < 0 || newX >= gameStateSHM->tableWidth || newY < 0 || newY >= gameStateSHM->tableHeight)
    return false;

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

static bool checkMoveAvailability(int indexPlayer, gameStateSHMStruct * gameStateSHM){

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

void createPlayer(gameStateSHMStruct * gameStateSHM, int playerIndex, int columns, int cellWidth, int cellHeight, char * playerName, pid_t pid){
  playerSHMStruct * player = &gameStateSHM->playerList[playerIndex];
    strncpy(player->playerName, playerName, sizeof(player->playerName) - 1);
    player->playerName[sizeof(player->playerName) - 1] = '\0';

    int row = playerIndex / columns;
    int col = playerIndex % columns;

    int x_ini = col * cellWidth;
    int y_ini = row * cellHeight;

    int x = x_ini + cellWidth / 2;
    int y = y_ini + cellHeight / 2;

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

// Verifica si un movimiento es válido (Usa master) (Usar funciones static)
bool validAndApplyMove(unsigned char mov, int indexPlayer, gameStateSHMStruct * gameStateSHM){
    //Si es borde, retornar -1 (se quiere mover para afuera)
    if(indexPlayer < 0 || indexPlayer >= 9){
      perror("Indice invalido para acceder a jugador");
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

// Encuentra mejor movimiento del player (Usa player) (Usar funciones static)
unsigned char findBestMove(int indexPlayer, gameStateSHMStruct *gameState) {
  int currentX = gameState->playerList[indexPlayer].tableX;
  int currentY = gameState->playerList[indexPlayer].tableY;

  int maxScore = -1;
  unsigned char bestMove = 8; // inválido por default

  for (unsigned char mov = 0; mov < 8; mov++) {
      int newX = currentX;
      int newY = currentY;

      movCases(mov, &newX, &newY);

      if (!isMoveLegal(newX, newY, gameState))
          continue;

      int idx = newY * gameState->tableWidth + newX;
      int score = gameState->tableStartPointer[idx];

      if (score > maxScore) {
          maxScore = score;
          bestMove = mov;
      }
  }

  return bestMove;
}

//Verifica si el jugador esta bloqueado
bool playerIsBlocked(){
  return false;
}

// Verifica si un jugador está bloqueado
void printPlayer(int playerIndex, int status,gameStateSHMStruct * gameStateSHM){
  printf("Player %s (%d) exited (%d) with score of %d / %d / %d\n", gameStateSHM->playerList[playerIndex].playerName, playerIndex, status, gameStateSHM->playerList[playerIndex].score,gameStateSHM->playerList[playerIndex].validMoveQty, gameStateSHM->playerList[playerIndex].invalidMoveQty);
  return;
}


