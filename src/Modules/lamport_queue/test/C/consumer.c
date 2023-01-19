#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "../queue/lamport_queue.h"

double get_random() { return ((double)rand() / (double)RAND_MAX); }

struct LamportQueue* lq;
int shmid;

double laplace_variable(double b){
  // srand(); // randomize seed
  double uniform_1 = ((double)rand() / (double)RAND_MAX);

  double uniform_2 = ((double)rand() / (double)RAND_MAX);
  while (uniform_2 == 0){
      uniform_2 = ((double)rand() / (double)RAND_MAX);
  }
  double lap = log(uniform_1 / uniform_2);
  lap = lap * (b);
  return lap;
}

double min(double a, double b){
  if (a < b){
    return a;
  }
  else{
    return b;
  }
}

double max(double a, double b){
  if (a > b){
    return a;
  }
  else{
    return b;
  }
}

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

  // simulating MinesVPN behavior
  int queue_size, data_size;
  double D = 100;
  double epsilon = 1;
  int B_max = 1000;
  int B_min = 0;
  double laplace_noise, DP_decision;

  while (1){
    // get the queue size
    queue_size = lamport_queue_size(queue);
    laplace_noise = laplace_variable(D/epsilon); 

    DP_decision = max(B_min, min(B_max, queue_size + laplace_noise));
    data_size = (ssize_t)min(queue_size, DP_decision);
    char* data = (char*) malloc(data_size);
    if(data_size == 0){
      continue;
    }
    if(lamport_queue_pop(queue, data, data_size) < 0){
      perror("lamport_queue_pop");
      exit(1);
    }
    free(data);
  } 

  return 0;
}