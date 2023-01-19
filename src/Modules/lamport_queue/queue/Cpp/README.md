# Lamport Queue Implementation in C++
In this document, we explain the structure, interfaces, and testing of the lamport queue implementation. The queue is defined as a class, where it can be used in different projects. To use the queue, include the header file `LamportQueue.hpp` in the code. 
The code is structured as follows:
``` C
class LamportQueue{
  public:
    explicit LamportQueue();
    int push(uint8_t* elem, size_t elem_size);
    int pop(uint8_t* elem, size_t elem_size);
    int size();
    int free_space();

  private:
    std::atomic<size_t> front_;
    std::atomic<size_t> back_;
    size_t cached_front_;
    size_t cached_back_;
    uint8_t data_[BUFFER_SIZE];

    int mod(int a, int b);
    int get_queue_size_local(size_t f, size_t b);
    int get_free_space_local(size_t f, size_t b);
};
```
Where the front_ determines tail of queue, where the consumer decqueues data; the back_ determines the head of the queue, the the producer enqueues data. Two cache variables, cached_front_ and chached_back_, are used to optimize queue performance by using previously accessed pointers if they are still vaild.

The general intuition behind the implementation is that in a SCSP queue, the front and back pointers can only be modified by the consumer and the producer respectively. Thefore, there is no need for guaranteeing sequential access when they are read by their correpsoing functions. For more information you can take a look at the paper, where the proof of correctness is provided.

## Queue Interfaces
An instance of a LamportQueue has 4 interfaces. The name of interfaces are self-explanatory.

```C
int push(uint8_t* elem, size_t elem_size)
```
The size of element to push shoud be within the following range: `0 < elem_size < BUF_SIZE`.
In case of failure, where there is not enough space in the queue, the interface returns `-1`.

```C
int pop(uint8_t* elem, size_t elem_size)
```
The size of element to push shoud be within the following range: `0 < elem_size < BUF_SIZE`.
In case of failure, where there is not enough data in the queue, the interface returns `-1`.  
The pointer that is passed to the queue to get data, `char* elem`, should point to a **pre allocated** memory region with a size of at least `elem_size`.
