#include "utils.h"

// Coordenadas de dirección (arriba, arriba-der, derecha, etc.)
//crea jugador
void createPlayer(){
  return;
}

// Verifica si un movimiento es válido (Usa master) (Usar funciones static)
bool validAndApplyMove(unsigned mov, int index, gameStateSHMStruct * gameStateSHM){
  return true;
}

// Encuentra mejor movimiento del player (Usa player) (Usar funciones static)
unsigned char findBestMove(){
  return 0;
}

//Verifica si el jugador esta bloqueado
bool playerIsBlocked(){
  return false;
}

static bool validMove(unsigned char mov, int playerID, gameStateSHMStruct * gameStateSHM){
  return true;
}

// Aplica un movimiento al jugador 
static void applyMovement(int playerID, unsigned char mov, gameStateSHMStruct * gameStateSHM){
  return;
}

// Verifica si un jugador está bloqueado
void printPlayer(int playerIndex, gameStateSHMStruct * gameStateSHM){
  printf("Player %s (%d) exited (%d) with score of %d / %d / %d\n", gameStateSHM->playerList[playerIndex].playerName, playerIndex, 0, gameStateSHM->playerList[playerIndex].score,gameStateSHM->playerList[playerIndex].validMoveQty, gameStateSHM->playerList[playerIndex].invalidMoveQty);
  return;
}