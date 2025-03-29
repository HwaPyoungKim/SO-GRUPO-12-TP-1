#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct {
  char playerName[16]; // Nombre del jugador
  unsigned int score; // Puntaje
  unsigned int invalidMoveQty; // Cantidad de solicitudes de movimientos inválidas realizadas
  unsigned int validMoveQty; // Cantidad de solicitudes de movimientos válidas realizadas
  unsigned short tableX, tableY; // Coordenadas x e y en el tablero
  pid_t playerPID; // Identificador de proceso
  bool isBlocked; // Indica si el jugador está bloqueado
} XXX;

typedef struct {
  unsigned short tableLength; // Ancho del tablero
  unsigned short tableHeight; // Alto del tablero
  unsigned int playerQty; // Cantidad de jugadores
  XXX playerList[9]; // Lista de jugadores
  bool gameState; // Indica si el juego se ha terminado
  int tableStartPointer[]; // Puntero al comienzo del tablero. fila-0, fila-1, ..., fila-n-1
} YYY;
