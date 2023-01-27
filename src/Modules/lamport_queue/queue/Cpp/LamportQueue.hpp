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
   * @param buffer byteArray
   * @param length number of bytes to copy from the given byteArray
   * @return -1 if failed, 0 if successful. Can fail if queue does not have
   * enough space
   */
  int push(uint8_t *buffer, size_t length);

  /**
   *
   * @param buffer The empty buffer to be filled
   * @param length Number of bytes to fill in that buffer (caller has to
   * ensure the buffer has enough space)
   * @return -1 if not enough bytes in the queue (will not partially fill the
   * buffer)
   */
  int pop(uint8_t *buffer, size_t length);

  /**
   * @brief Gives current size of the queue
   * @return The current size of the queue (number of bytes available)
   */
  size_t size();

  size_t freeSpace();

  /**
   * @brief The current client this queue is bound to
   */
  char clientAddress[16] = "";
  char clientPort[6] = "";
  char serverAddress[16] = "";
  char serverPort[6] = "";
  uint64_t queueID;

private:
  std::atomic<size_t> front;
  std::atomic<size_t> back;
  size_t cachedFront;
  size_t cachedBack;
  uint8_t queueStorage[BUFFER_SIZE]{};

  size_t mod(ssize_t a, ssize_t b);

  size_t getQueueSizeLocal(size_t f, size_t b);

  size_t getFreeSpaceLocal(size_t f, size_t b);
};

#endif //MINESVPN_LAMPORTQUEUE_H

