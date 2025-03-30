#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char *argv[]){
  //recibe como parametro el ancho y alto del tablero


  //Se conecta con ambas memorias compartidas

  //Mientras no esté bloqueado, consultar el estado del tablero de forma sincronizada entre el
  //máster y el resto de jugadores y enviar solicitudes de movimientos al máster.

  //Ejemplo

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
        return 1;
    }

    srand(time(NULL) ^ getpid());

    // Simulamos el envío de 20 movimientos
    for (int i = 0; i < 20; i++) {
        unsigned char movimiento = rand() % 8; // [0-7], direcciones válidas
        write(1, &movimiento, sizeof(movimiento)); // escribir al stdout → máster lo recibe por el pipe
        usleep(3000);  // Espera 300 ms para no spamear
    }

    return 0;
}



///////////////////////////////////////////////////////////////////////////////////////////

