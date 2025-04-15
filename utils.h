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
    char view_path[MAX_LENGTH];
    char playerPaths[MAX_PLAYERS][MAX_LENGTH];
    int playerCount;
} config_t;

void createPlayer(gameStateSHMStruct * gameStateSHM, int playerIndex, int columns, int cellWidth, int cellHeight, char * playerName, pid_t pid);

bool validAndApplyMove(unsigned char mov, int index, gameStateSHMStruct * gameStateSHM);

int findWinner(gameStateSHMStruct * gameStateSHM);

void printPlayer(int playerIndex, int status,gameStateSHMStruct * gameStateSHM);

void printWinner(int playerIndex, gameStateSHMStruct * gameStateSHM);

#endif