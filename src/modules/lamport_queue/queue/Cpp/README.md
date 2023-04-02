# Lamport Queue Implementation in C++

In this document, we explain the structure, interfaces, and testing of the
lamport queue implementation. The queue is defined as a class, where it can be
used in different projects. To use the queue, include the header
file `LamportQueue.hpp` in the code.
The code is structured as follows:

``` C
class LamportQueue{
  public:
    explicit LamportQueue(uint64_t ID);
    int push(uint8_t* elem, size_t elem_size);
    int pop(uint8_t* elem, size_t elem_size);
    int size();
    int freeSpace();
    
    char clientAddress[16] = "";
    char clientPort[6] = "";
    uint64_t ID;

  private:
    std::atomic<size_t> front;
    std::atomic<size_t> back;
    size_t cachedFront;
    size_t cachedBack;
    uint8_t queueStorage[BUFFER_SIZE];

    int mod(int a, int b);
    int getQueueSizeLocal(size_t f, size_t b);
    int getFreeSpaceLocal(size_t f, size_t b);
};
```

Where the front determines tail of queue, where the consumer dequeues data;
the back determines the head of the queue, the producer enqueues data. Two
cache variables, cachedFront and cached_back_, are used to optimize queue
performance by using previously accessed pointers if they are still valid.

The general intuition behind the implementation is that in a SCSP queue, the
front and back pointers can only be modified by the consumer and the
producer respectively. Therefore, there is no need for guaranteeing
sequential access when they are read by their corresponding functions. For more
information you can take a look at the paper, where the proof of correctness is
provided.

## Queue Interfaces

An instance of a LamportQueue has 4 interfaces. The names of interfaces are
self-explanatory.

```C
int push(uint8_t* buffer, size_t length)
```

The size of element to push should be within the following range: `0 <
elem_size < BUF_SIZE`.
In case of failure, where there is not enough space in the queue, the interface
returns `-1`.

```C
int pop(uint8_t* buffer, size_t length)
```

The size of element to push should be within the following range: `0 <
elem_size < BUF_SIZE`.
In case of failure, where there is not enough data in the queue, the interface
returns `-1`.  
The pointer that is passed to the queue to get data, `char* elem`, should point
to a **pre allocated** memory region with a size of at least `elem_size`.

### Miscellaneous Information

The queue stores the following other information:

1. `clientAddress`: The address of the client currently corresponding to
   this queue. This is null for queues that do not have a client attached to
   them
2. `clientPort`: The port of the client currently corresponding to this
   queue. This is null for queues that do not have a client attached to them
3. `ID`: The unique ID of this queue. This is used for inter-process
   signalling, when a new client connects or a client terminates the connection
