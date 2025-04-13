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
  char playerName[16]; // Nombre del jugador
  unsigned int score; // Puntaje
  unsigned int invalidMoveQty; // Cantidad de solicitudes de movimientos inválidas realizadas
  unsigned int validMoveQty; // Cantidad de solicitudes de movimientos válidas realizadas
  unsigned short tableX, tableY; // Coordenadas x e y en el tablero
  pid_t playerPID; // Identificador de proceso
  bool isBlocked; // Indica si el jugador está bloqueado
} playerSHMStruct;

typedef struct {
  unsigned short tableWidth; // Ancho del tablero
  unsigned short tableHeight; // Alto del tablero
  unsigned int playerQty; // Cantidad de jugadores
  playerSHMStruct playerList[9]; // Lista de jugadores
  bool gameState; // Indica si el juego se ha terminado
  int tableStartPointer[]; // Puntero al comienzo del tablero. fila-0, fila-1, ..., fila-n-1
} gameStateSHMStruct;


///////////////////////////////////


typedef struct {
  sem_t A; //semPendingPrints; // Se usa para indicarle a la vista que hay cambios por imprimir
  sem_t B; //semFinishedPrints; // Se usa para indicarle al master que la vista terminó de imprimir

  sem_t writerPrivilege; // Mutex para evitar inanición del master al acceder al estado
  sem_t masterPlayerMutex; //currentGameState; // Mutex para el estado del juego
  sem_t playersReadingCountMutex; // Mutex para la siguiente variable // Solo lo usa el player para el checkeo con F
  unsigned int playersReadingCount; // Cantidad de jugadores leyendo el estado
} gameSyncSHMStruct;

