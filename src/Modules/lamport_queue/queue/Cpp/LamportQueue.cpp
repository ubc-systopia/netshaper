/*
  Created by Amir Sabzi
*/

#include "LamportQueue.hpp"

LamportQueue::LamportQueue() {
  front_ = 0;
  back_ = 0;
  cached_front_ = 0;
  cached_back_ = 0;
}

// LamportQueue::~LamportQueue() {
//   delete data_;
// }

int LamportQueue::push(uint8_t *elem, size_t elem_size) {
  assert(elem_size < BUFFER_SIZE && elem_size > 0);

  size_t b, f;
  b = this->back_.load(std::memory_order_relaxed);
  f = this->cached_front_;
  size_t free_space = this->get_free_space_local(f, b);
  if (free_space < elem_size) {
    this->cached_front_ = f = this->front_.load(std::memory_order_acquire);
  }
  free_space = this->get_free_space_local(f, b);
  if (free_space < elem_size) {
    return -1;
  }
  std::memcpy(this->data_ + b, elem, elem_size);
  this->back_.store((b + elem_size) % BUFFER_SIZE, std::memory_order_release);
  return 0;
}

int LamportQueue::pop(uint8_t *elem, size_t elem_size) {
  assert(elem_size < BUFFER_SIZE && elem_size > 0);

  size_t b, f;
  f = this->front_.load(std::memory_order_relaxed);
  b = this->cached_back_;
  size_t queue_size = this->get_queue_size_local(f, b);
  if (queue_size < elem_size) {
    this->cached_back_ = b = this->back_.load(std::memory_order_acquire);
  }
  queue_size = this->get_queue_size_local(f, b);
  if (queue_size < elem_size) {
    return -1;
  }
  std::memcpy(elem, this->data_ + f, elem_size);
  this->front_.store((f + elem_size) % BUFFER_SIZE, std::memory_order_release);
  return 0;
}

size_t LamportQueue::size() {
  size_t f = this->front_.load(std::memory_order_relaxed);
  size_t b = this->back_.load(std::memory_order_acquire);
  return this->get_queue_size_local(f, b);
}

size_t LamportQueue::free_space() {
  size_t f = this->front_.load(std::memory_order_relaxed);
  size_t b = this->back_.load(std::memory_order_acquire);
  return this->get_free_space_local(f, b);
}

size_t LamportQueue::mod(ssize_t a, ssize_t b) {
  ssize_t r = a % b;
  return r < 0 ? r + b : r;
}

size_t LamportQueue::get_free_space_local(size_t f, size_t b) {
  return BUFFER_SIZE - this->mod((b - f), BUFFER_SIZE) - 1;
}

size_t LamportQueue::get_queue_size_local(size_t f, size_t b) {
  return this->mod((b - f), BUFFER_SIZE);
}