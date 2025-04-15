#include "utils.h"

#define GAME_STATE_SHM_NAME "/game_state"
#define GAME_SYNC_SHM_NAME "/game_sync"

static void endRead(gameSyncSHMStruct *sync);
static void beginRead(gameSyncSHMStruct *sync);

int main(int argc, char *argv[]){

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
        return 1;
    }

    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
  
    size_t totalSize = sizeof(gameStateSHMStruct) + sizeof(int) * width * height;

    int gameStateFD = shm_open(GAME_STATE_SHM_NAME, O_RDONLY, 0666);
    if (gameStateFD == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    int gameSyncFD = shm_open(GAME_SYNC_SHM_NAME, O_RDWR, 0666);
    if (gameSyncFD == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    gameStateSHMStruct *gameStateSHM = mmap(NULL, totalSize, PROT_READ, MAP_SHARED, gameStateFD, 0);
    if (gameStateSHM == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    gameSyncSHMStruct *gameSyncSHM = mmap(NULL, sizeof(gameSyncSHMStruct), PROT_READ | PROT_WRITE,    MAP_SHARED, gameSyncFD, 0);
    if (gameSyncSHM == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    bool stillPlaying = true;
    srand(time(NULL) ^ getpid());

    while(stillPlaying){
        beginRead(gameSyncSHM);

        if(!gameStateSHM->gameState){
            stillPlaying = false;
        }
        unsigned char movimiento = rand() % 8;
        endRead(gameSyncSHM);
        write(1, &movimiento, sizeof(movimiento));
    }

    munmap(gameStateSHM, totalSize);
    munmap(gameSyncSHM, sizeof(gameSyncSHMStruct));

    close(gameStateFD);
    close(gameSyncFD);

    return 0;
}

static void beginRead(gameSyncSHMStruct *sync) {
    sem_wait(&sync->writerPrivilege);
    sem_post(&sync->writerPrivilege);
    
    sem_wait(&sync->playersReadingCountMutex);
    sync->playersReadingCount++;
    if (sync->playersReadingCount == 1) sem_wait(&sync->masterPlayerMutex);
    sem_post(&sync->playersReadingCountMutex);
    
}

static void endRead(gameSyncSHMStruct *sync) {
    sem_wait(&sync->playersReadingCountMutex);
    sync->playersReadingCount--;
    if (sync->playersReadingCount == 0) sem_post(&sync->masterPlayerMutex);
    sem_post(&sync->playersReadingCountMutex);
}



