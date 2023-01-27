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

class LamportQueue {
public:
  /**
   * Default constructor
   */
  explicit LamportQueue(uint64_t queueID);

  /**
   *
   * @param elem byteArray
   * @param elem_size number of bytes to copy from the given byteArray
   * @return -1 if failed, 0 if successful. Can fail if queue does not have
   * enough space
   */
  int push(uint8_t *elem, size_t elem_size);

  /**
   *
   * @param elem The empty buffer to be filled
   * @param elem_size Number of bytes to fill in that buffer (caller has to
   * ensure the buffer has enough space)
   * @return -1 if not enough bytes in the queue (will not partially fill the
   * buffer)
   */
  int pop(uint8_t *elem, size_t elem_size);

  /**
   * @brief Gives current size of the queue
   * @return The current size of the queue (number of bytes available)
   */
  size_t size();

  size_t free_space();

  /**
   * @brief The current client this queue is bound to
   */
  char clientAddress[16];
  char clientPort[6];
  uint64_t queueID;

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

