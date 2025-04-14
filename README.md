# SO-GRUPO-12-TP-1

## Autores
Tomas Kaneko
Hwa Pyoung Kim
Roman Berruti

## Aplicación
Nuestro proyecto consiste de los siguientes archivos:

* master: Crea jugadores y la vista. Se encarga de procesar las solicitudes de movimiento de los jugadores.
* vista: Imprime el tablero del juego con los jugadores y sus respectivos puntajes.
* player: solicita movimientos al master mediante el pipe.
* sharedMemory: definicion de structs para las memorias compartidas
* utils: Funciones que se utilizan para la logica del juego y los prints al final del juego.
  
## Compilación
Ejecutar make o make all para el compilado de los archivos. Asegurarse de tener instalado ncurses con apt update y apt install libncurses5-dev libncursesw5-dev en su contenedor de docker. Luego de la ejecución se generarán los siguientes archivos:

* Master
* Vista
* Player1
* Player2
* Player3
* Player4
* Player5
* Player6
* Player7
* Player8
* Player9

Si desea remover los mismos, ejecute make clean en el mismo directorio donde fue realizada la compilación.

## Ejecución
Para correr el programa ejecutar el archivo Master corriendo ./Master junto a los siguientes parametros. 
* -w width: Ancho del tablero (opcional)
* -h height: Alto del tablero (opcional)
* -d delay: milisegundos que espera el máster cada vez que se imprime el estado.(opcional)
* -t timeout: Timeout en segundos para recibir solicitudes de movimientos válidos.(opcional)
* -s seed: Semilla utilizada para la generación del tablero (opcional) 
* -v view: Ruta del binario de la vista. (opcional)
* -p player1 player2: Ruta/s de los binarios de los jugadores. (requerido y maximo 9)

Por ejemplo:
```MakeFile
  ./Master -p player1 -w 15 -h 15 -d 400 -t 5
 ```
Para ejecutar con vista:
```Makefile
  ./Master -p player1 player2 -v vista
```
