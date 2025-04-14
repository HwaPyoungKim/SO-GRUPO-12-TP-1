#ifndef UTILS_H
#define UTILS_H

#include "sharedMemory.h" 

#define MAX_PLAYERS 9
#define ERROR_VALUE -1
#define MAX_LENGTH 20

typedef struct {
    int width;
    int height;
    int delay;
    int timeout;
    int seed;
    char view_path[MAX_LENGTH]; // puede ser NULL
    char playerPaths[MAX_PLAYERS][MAX_LENGTH];
    int playerCount;
} config_t;

//crea jugador
void createPlayer(gameStateSHMStruct * gameStateSHM, int playerIndex, int columns, int cellWidth, int cellHeight, char * playerName, pid_t pid);

// Verifica si un movimiento es válido (Usa master) (Usar funciones static)
bool validAndApplyMove(unsigned char mov, int index, gameStateSHMStruct * gameStateSHM);

// Verifica si un jugador está bloqueado
void printPlayer(int playerIndex, int status,gameStateSHMStruct * gameStateSHM);

#endif