/*
  Created by Amir Sabzi
*/

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/shm.h>
#include <unistd.h>
#include <cmath>
#include "../../queue/Cpp/LamportQueue.hpp"

double get_random() { return ((double) rand() / (RAND_MAX)); }

double laplace_variable(double b){
  // srand(); // randomize seed
  double uniform_1 = get_random();

  double uniform_2 = get_random();
  while (uniform_2 == 0){
      uniform_2 = get_random();
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
    std::cout << "Usage: " << argv[0] << " <number of elements>" << std::endl;
    return -1;
  }

  // Initialize the shared memory
  int shmid = shmget(SHM_KEY, sizeof(class LamportQueue), IPC_CREAT | 0666);
  if (shmid < 0) {
    perror("shmget");
    return -1;
  }
  //Lamport queue is constructed once in the initializer, we here only create a pointer to it
  LamportQueue *lq = (LamportQueue *) shmat(shmid, NULL, 0);
  if(lq == (void *) -1){
    perror("shmat");
    return -1;
  }


  // simulating MinesVPN behavior
  int queue_size, data_size;
  double D = 100;
  double epsilon = 1;
  int B_max = 1000;
  int B_min = 0;
  double laplace_noise, DP_decision;

  while(1){
    queue_size = lq->size();
    // std::cout << "Consumer: queue_size: " << queue_size << std::endl;
    // std::cout << "Consumer: buffer_free_size: " << lq->free_space() << std::endl;
    laplace_noise = laplace_variable(epsilon / D);
    DP_decision = max(B_min, min(B_max, queue_size + laplace_noise));
    data_size = (size_t) min(queue_size, DP_decision);
    uint8_t *data = new uint8_t[data_size];
    if(data_size == 0){
      continue;
    }

    std::cout << "Consumer: data_size: " << data_size << std::endl;
    if(lq->pop(data, data_size) == -1){
      std::cout << "Queue is empty" << std::endl;
      return -1;
    }
    delete data;
  }
  return 0;
}