# Peer 1 Documentation
## Unshaped component
In our prototype, a TCP client is connected to the peer 1. The unshaped compnenet receives data from the client and sends it to the shaped component. It also receives data from the shaped component and sends it to the client.

### Initialization
Each MinesVPN middlebox can support up to a maximum number of clients, `int numstreams`. Upon running the middlebox application, we create `numstreams` pair of queues, and initialize them over a shared memory region. We further map these queues to client sockets on demand. 

### Connection Establishment and Queue Assignment
To create a local TCP server, we create an instance of `UnshapedTransciever::Receiver` class and pass the `receivedUnshapedData` as a call back function to it. This function is called whenever the peer 1 receives data from a client. 
As we mentioned in the MinesVPN documentation, multiple clients can connect to peer 1. 
For each new client, the peer 1 creates a new pair of queues (one for traffic from client to the server, and the other for traffic from server to the client) and assigns them to the client. 
To maintain the mapping between clients and queues, we use a C++ unordered map, where the key is the client's socket number and the value is a pointer to its dedicated pair of queues.
Similarly, we create a separate map from the queue pair to the client's socket number.
```cpp
std::unordered_map<int, queuePair> *socketToQueues;
std::unordered_map<queuePair, int, queuePairHash> *queuesToSocket;
```

### Data Transmission
Upon receiving data from clients, the `receivedUnshapedData` function first checks if a pair of queues is already assigned to the client. If yes, it enqueues received data to the corresponding queue. Otherwise, it creates a new pair of queues and assigns them to the client.  

To receive data from the shaped component and send it to the client, we create a separate thread running a function called `receivedResponse`.
This function periodically checks if there is any data in the queue from the shaped component to unshaped one. If yes, it dequeues the data and sends it to the client.

## Shaped component
The shaped components manages the QUIC connection between two middleboxes. It also performs the shaping operation on the data from unshaped component.

