#include <stdio.h>
#include <stdlib.h>
#include "../queue/lamport_queue.h"


int main(int argc, char *argv[]){
    if(argc != 1){
        printf("Usage: %s <data>\n", argv[0]);
        exit(1);
    }

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
    while (1){
      // selecting a random number between 0 and 200
      int data_size = rand() % 200 + 1;
      char* data = (char*) malloc(data_size);
      memset(data, 'a', data_size);
      int queue_size_before= lamport_queue_size(queue);
      int buffer_free_size = BUF_SIZE - queue_size_before;
      if (buffer_free_size > data_size){
        if(lamport_queue_push(queue, data, data_size) < 0){
          perror("lamport_queue_push");
          exit(1);
        }
      }
      else{
        continue;
      }
      free(data);
    }
    return 0;
}