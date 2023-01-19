/*
  Created by Amir Sabzi
*/

#ifndef MINESVPN_LAMPORTQUEUE_H
#define MINESVPN_LAMPORTQUEUE_H

#include <cassert>
#include <iostream>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <atomic>
#include <cstring>

#define BUFFER_SIZE 1000
#define SHM_KEY 0x1234

class LamportQueue {
public:
  explicit LamportQueue();

  // ~LmportQueue();
  int push(uint8_t *elem, size_t elem_size);

  int pop(uint8_t *elem, size_t elem_size);

  size_t size();

  size_t free_space();

private:
  std::atomic<size_t> front_;
  std::atomic<size_t> back_;
  size_t cached_front_;
  size_t cached_back_;
  uint8_t data_[BUFFER_SIZE]{};

  size_t mod(ssize_t a, ssize_t b);

  size_t get_queue_size_local(size_t f, size_t b);

  size_t get_free_space_local(size_t f, size_t b);
};

#endif //MINESVPN_LAMPORTQUEUE_H

