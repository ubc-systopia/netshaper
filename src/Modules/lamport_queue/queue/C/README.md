## Lamport Queue Documentation

Here we explain the implementation of lamport single-producer, single-consumer (
SCSP) shared queue, its interfaces, and our tests to guarantee its correctness.

### Implementation

The queue structure is as follows:

```C
struct lamport_queue{
    atomic_int front_;
    atomic_int back_;
    int cached_front_;
    int cached_back_;
    char data_[BUF_SIZE];
};
```

Where the `front_` determines tail of queue, where the consumer dequeues data;
the `back_` determines the head of the queue, the producer enqueues data.
Two cache variables, `cached_front_` and `chached_back_`, are used to
optimize queue performance by using previously accessed pointers if they are
still valid.

The general intuition behind the implementation is that in a SCSP queue, the
front and back pointers can only be modified by the consumer and the
producer respectively. Therefore, there is no need for guaranteeing sequential
access when they are read by their corresponding functions. For more information
you can take a look at the [paper](https://hal.inria.fr/hal-00911893/document),
where the proof of correctness is provided.

### Queue interfaces

To use queue simply include the queue header file (`queue/lamport_queue.h`) in
your program.

The queue has three interfaces:

```C
int lamport_queue_push(struct lamport_queue *queue, char* elem, int elem_size)
```

The size of element to push should be within the following
range: `0 < elem_size < BUF_SIZE`.
In case of failure, where there is not enough space in the queue, the interface
returns `-1`.

```C
int lamport_queue_pop(struct lamport_queue *queue, char* elem, int elem_size)
```

The size of element to push should be within the following
range: `0 < elem_size < BUF_SIZE`.
In case of failure, where there is not enough data in the queue, the interface
returns `-1`.  
The pointer that is passed to the queue to get data, `char* elem`, should point
to a **pre allocated** memory region with a size of at least `elem_size`.

```C
int lamport_queue_size(struct lamport_queue *queue)
```

It simply returns the current size of the queue.

### Testing

To test the queue, we created a test setup, where a producer program generates
data with random sizes and pushes them into the queue. A separate consumer
program dequeues random-sized portions of data in the queue. Programs are
executed asynchronously in a running loop.

To run the test use following commands:

```
cd test
./test.sh
```

The queue passed the test without any errors observed so far.







