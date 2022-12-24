#include <stdio.h>
#include <stdlib.h>
#include "../queue/lamport_queue.h"


int main(){
    // Initialize the shared memory
    int shmid = shmget(SHM_KEY, sizeof(struct lamport_queue), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    struct lamport_queue *queue = shmat(shmid, NULL, 0);
    if (queue == (void *) -1) {
        perror("shmat");
        exit(1);
    }
    // Initialize the queue
    if(lamport_queue_init(queue) < 0){
        perror("lamport_queue_init");
        exit(1);
    }
    return 0;  
}