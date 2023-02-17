/*
  Created by Amir Sabzi
*/

#include "LamportQueue.hpp"

LamportQueue::LamportQueue(uint64_t queueID) : ID(queueID) {
  front = 0;
  back = 0;
  cachedFront = 0;
  cachedBack = 0;
}

// LamportQueue::~LamportQueue() {
//   delete queueStorage;
// }

int LamportQueue::push(uint8_t *buffer, size_t length) {
  assert(length < BUFFER_SIZE && length > 0);

  size_t b, f;
  b = this->back.load(std::memory_order_relaxed);
  f = this->cachedFront;
  size_t freeSpace = this->getFreeSpaceLocal(f, b);
  if (freeSpace < length) {
    this->cachedFront = f = this->front.load(std::memory_order_acquire);
  }
  freeSpace = this->getFreeSpaceLocal(f, b);
  if (freeSpace < length) {
    return -1;
  }
  if (b + length > BUFFER_SIZE) {
    auto size1 = BUFFER_SIZE - b;
    std::memcpy(this->queueStorage + b, buffer, size1);
    std::memcpy(this->queueStorage, buffer + size1, length - size1);
  } else {
    std::memcpy(this->queueStorage + b, buffer, length);
  }
  this->back.store((b + length) % BUFFER_SIZE, std::memory_order_release);
  return 0;
}

int LamportQueue::pop(uint8_t *buffer, size_t length) {
  assert(length < BUFFER_SIZE && length > 0);

  size_t b, f;
  f = this->front.load(std::memory_order_relaxed);
  b = this->cachedBack;
  size_t queueSize = this->getQueueSizeLocal(f, b);
  if (queueSize < length) {
    this->cachedBack = b = this->back.load(std::memory_order_acquire);
  }
  queueSize = this->getQueueSizeLocal(f, b);
  if (queueSize < length) {
    return -1;
  }
  if (f + length > BUFFER_SIZE) {
    auto size1 = BUFFER_SIZE - f;
    std::memcpy(buffer, this->queueStorage + f, size1);
    std::memcpy(buffer + size1, this->queueStorage, length - size1);
  } else {
    std::memcpy(buffer, this->queueStorage + f, length);
  }
  this->front.store((f + length) % BUFFER_SIZE, std::memory_order_release);
  return 0;
}

size_t LamportQueue::size() {
  size_t f = this->front.load(std::memory_order_relaxed);
  size_t b = this->back.load(std::memory_order_acquire);
  return this->getQueueSizeLocal(f, b);
}

void LamportQueue::clear() {
  front = back = cachedFront = cachedBack = 0;
}

size_t LamportQueue::freeSpace() {
  size_t f = this->front.load(std::memory_order_relaxed);
  size_t b = this->back.load(std::memory_order_acquire);
  return this->getFreeSpaceLocal(f, b);
}

size_t LamportQueue::mod(ssize_t a, ssize_t b) {
  ssize_t r = a % b;
  return r < 0 ? r + b : r;
}

size_t LamportQueue::getFreeSpaceLocal(size_t f, size_t b) {
  return BUFFER_SIZE - this->mod(b - f, BUFFER_SIZE) - 1;
}

size_t LamportQueue::getQueueSizeLocal(size_t f, size_t b) {
  return this->mod(b - f, BUFFER_SIZE);
}