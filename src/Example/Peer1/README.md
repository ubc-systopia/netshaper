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
The shaped components manages the QUIC connection between two middleboxes. It also performs the shaping operation on the data received from unshaped component. 
This component is defined as a process with two threads.
One thread is responsible for sending data and the other measures the current queue sizes and determines the amount of data the sending thread is allowed to transmit. The DP threads updates an atomic variable called sendingCredit and the sending thread reads this variable to determine the amount of data it can send. After sending data, the sending thread updates the sendingCredit variable by decreasing it.
The following figure represents the architecture of the shaped component.
![ShapedComponent architecture](../../../docs/figures/shaped_component_arch.png)



### Connection Establishment and Queue Assignment
As we mentioned before, the shaped component is responsible for establishing a QUIC connection between two middleboxes.
QUIC, like TCP, follows a client-server model. In our protype, the peer 1 (middlebox placed in the client side) is the client and the peer 2 (middlebox placed in the server side) is the server.
Therefore, the peer 1 creates a QUIC connection to the peer 2. 
This is one of the main differences between peer 1 shaped component and peer 2 shaped component.
Every QUIC connection carries multiple streams. We define a control stream between two middleboxes to exchange control messages.
The control messages are used to identify the dummy and the real streams.
The following is the control message format.
```cpp
enum StreamType {
  Control, Dummy, Data
};
struct controlMessage {
  uint64_t streamID;
  enum StreamType streamType;
  char srcIP[16];
  int srcPort;
  char destIP[16];
  int destPort;
};
```
The last 4 variables in control message are used to identify the real stream.
In the first message from peer 1 to peer 2, the `streamType` is set to control to identify the control stream. The second control message identifies the dummy stream. 

We map every queue to separate QUIC stream. The mapping is defined in the `queuesToStream` and `streamToQueues` maps. 
Similar to unshaped component, we create two queues for each stream. One for outgoing data and the other for incoming data.
```cpp
std::unordered_map<queuePair, MsQuicStream *, queuePairHash> *queuesToStream;
std::unordered_map<MsQuicStream *, queuePair> *streamToQueues;
```
### Data Transmission
The sender thread and the DP thead are executed periodically with a period of `T'` and `T` respectively. 
To make sure while the DP thread is updating the sending credit, the sender thread doesn't modify it we use a atomic variable called `sendingCredit`. Each of these threads can only read and write to this variable with acquire and release semantics respectively. 

For now, the sender thead searches over all queues and sends the data from any queue that has data.
We might consider prioritization of clients in later phases of the project.
Upon receiving data from Peer 2, `onResponse` function is called.
If the received data is from dummy streams, it simply drops the data. Otherwise, it enqueues the data to the corresponding queue.
