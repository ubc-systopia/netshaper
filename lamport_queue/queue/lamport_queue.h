// To understand the code, please read the paper: https://hal.inria.fr/hal-00911893/document

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <assert.h>

#define BUF_SIZE 1000
#define SHM_KEY 0x1234

int mod(int a, int b){
    int r = a % b;
    return r < 0 ? r + b : r;
}

struct lamport_queue{
    atomic_int front_;
    atomic_int back_;
    int cached_front_;
    int cached_back_;
    char data_[BUF_SIZE];
};

int lamport_queue_size(struct lamport_queue *queue){
    int f = atomic_load_explicit(&queue->front_, memory_order_relaxed);
    int b = atomic_load_explicit(&queue->back_, memory_order_acquire);
    return mod((b-f) , BUF_SIZE);
}

int get_queue_size_local(struct lamport_queue *queue, int f, int b){
    return mod((b-f) , BUF_SIZE);
}

int get_free_space_local(struct lamport_queue *queue, int f, int b){
    return BUF_SIZE - mod((b-f) , BUF_SIZE);
}

int lamport_queue_init(struct lamport_queue *queue)
{
       atomic_init(&queue->front_, 0);
       atomic_init(&queue->back_, 0);
       queue->cached_front_ = queue->cached_back_ = 0;
       return 0;
}

int lamport_queue_push(struct lamport_queue *queue, char* elem, int elem_size)
{
  // Checking the correctness of inputs
  assert(elem_size < BUF_SIZE && elem_size > 0);

  int b, f;
  b = atomic_load_explicit(&queue->back_, memory_order_relaxed);
  f = queue->cached_front_;
  int free_space = get_free_space_local(queue, f, b);
  if (free_space <= elem_size){
    queue->cached_front_ = f = atomic_load_explicit(&queue->front_, memory_order_acquire);
  }
  free_space = get_free_space_local(queue, f, b);
  if (free_space <= elem_size){
      return -1;
  }
  memcpy(queue->data_ + b, elem, elem_size);
  atomic_store_explicit(&queue->back_, (b + elem_size) % BUF_SIZE, memory_order_release);
  return 0;
}

int lamport_queue_pop(struct lamport_queue *queue, char* elem, int elem_size)
{
  // Checking the correctness of inputs
  assert(elem_size < BUF_SIZE && elem_size > 0);

  int b, f;
  f = atomic_load_explicit(&queue->front_, memory_order_relaxed);
  b = queue->cached_back_;
  int queue_size = get_queue_size_local(queue, f, b);
  if (queue_size < elem_size){
    queue->cached_back_ = b = atomic_load_explicit(&queue->back_, memory_order_acquire);
  }
  queue_size = get_queue_size_local(queue, f, b);
  if (queue_size < elem_size){
      return -1;
  }
  memcpy(elem, queue->data_ + f, elem_size);
  atomic_store_explicit(&queue->front_, (f + elem_size) % BUF_SIZE, memory_order_release);
  return 0;
}

