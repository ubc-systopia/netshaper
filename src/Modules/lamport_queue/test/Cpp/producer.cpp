/*
  Created by Amir Sabzi
*/


#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/shm.h>
#include "../../queue/Cpp/LamportQueue.hpp"
#include <unistd.h>

int main(int argc, char** argv) {
    if(argc != 1){
        std::cout << "Usage: " << argv[0] << " <number of elements>" << std::endl;
        return -1;
    }
    int shmid = shmget(SHM_KEY, sizeof(class LamportQueue), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        return -1;
    }

    // Lamport queue is constructed once in the initializer, we here only create a pointer to it
    LamportQueue *lq = (LamportQueue *) shmat(shmid, NULL, 0);
    if(lq == (void *) -1){
        perror("shmat");
        return -1;
    }
    while(1){
      size_t data_size = rand() % 200 + 1;
      uint8_t *data = new uint8_t[data_size];
      std::memset(data, 'a', data_size);
      int buffer_free_size = lq->free_space();
      // std::cout << "Producer: queue_size " << lq->size() << std::endl; 
      // std::cout << "Producer: buffer_free_size: " << buffer_free_size << std::endl;

      if(buffer_free_size > data_size){
        // std::cout << "Producer: data_size: " << data_size << std::endl;
        if(lq->push(data, data_size) == -1){
          std::cout << "Queue is full" << std::endl;
          return -1;
        }
      }else{
        continue;
      }
      delete data;
    }
    return 0;
}