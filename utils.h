#ifndef UTILS_H
#define UTILS_H

#include "sharedMemory.h" 

//crea jugador
void createPlayer();

// Verifica si un movimiento es válido (Usa master) (Usar funciones static)
bool validAndApplyMove(unsigned mov, int index, gameStateSHMStruct * gameStateSHM);

// Encuentra mejor movimiento del player (Usa player) (Usar funciones static)
unsigned char findBestMove(); 

//Verifica si el jugador esta bloqueado
bool playerIsBlocked();

// Verifica si un jugador está bloqueado
void printPlayer(int playerIndex, gameStateSHMStruct * gameStateSHM);

#endif