/*

  Created by Amir Sabzi
*/


#include <iostream>
#include <cstdlib>
#include <sys/shm.h>
#include <new>
#include "../../queue/Cpp/LamportQueue.hpp"

int main(){
  // Create a shared memory region for our lamport queue
  int shmid = shmget(SHM_KEY, sizeof(class LamportQueue), IPC_CREAT | 0666);
  if (shmid < 0) {
    perror("shmget");
    exit(1);
  }

  // Getting the address of this region in a void pointer
  void *shmaddr = shmat(shmid, NULL, 0);
  if(shmaddr == (void *) -1){
    perror("shmat");
    exit(1);
  }

  // Constructing a LamportQueue object in the shared memory region
  LamportQueue *lq = new (shmaddr) LamportQueue();

  if(lq == NULL){
    std::cout << "lq is null" << std::endl;
    exit(1);
  }
}