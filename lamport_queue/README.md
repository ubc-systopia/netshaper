## Lamport Queue Documentation
Here we explain the implementation of lamport single-producer, single-consumer (SCSP) shared queue, its interfaces, and our tests to guarantee its correctness. 

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
Where the `front_` determines tail of queue, where the consumer decqueues data; the `back_` determines the head of the queue, the the producer enqueues data.
Two cache variables, `cached_front_` and `chached_back_`, are used to optimize queue performance by using previously accessed pointers if they are still vaild.

The general intuition behind the implementation is that in a SCSP queue, the front and back pointers can only be modified by the consumer and the producer respectively. Thefore, there is no need for guaranteeing sequential access when they are read by their correpsoing functions. For more information you can take a look at the [paper](https://hal.inria.fr/hal-00911893/document), where the proof of correctness is provided. 
