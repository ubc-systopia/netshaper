# minesVPN example implementation

_Note: This folder contains an example implementation of minesVPN_

Data flow direction:  
`Client <--> UnshapedReceiver <--> ShapedSender <--> ShapedReceiver <-->
UnshapedSender <--> Server`

There are majorly 4 components in minesVPN, divided into 2 pairs:  
**Pair 1:** `UnshapedReceiver` and `ShapedSender`  
**Pair 2:** `ShapedReceiver` and `UnshapedSender`

- Each component runs in a separate process
- Each component in a pair has a shared memory with the other component in
  the pair. The total shared memory (and hence the total number of queues)
  is established when the program boots, and can't be changed after the
  program has been initialised.  
  The shared memory contains two types of queues:
  1. **Control Queue:** This queue contains control messages (e.g. when a
     new client connects). It is used to signal the other component on
     client joining/leaving events (not for data transmission)
  2. **Data Queue:** This queue contains data going two/from the client.
     There are two data queues per client (one for data moving in each
     direction).

### Unshaped Receiver

This component is based on the module `TCP Receiver` and acts as a TCP
Server. It does the following tasks:

- Listen for incoming TCP connections on a given port
- Assign a new data queue pair to each new client that joins
- Signal the `ShapedSender` whenever a new client joins, with the
  information on the assigned queue.
- Whenever a connected client sends data, push it to the shared data queue
  in the outwards (toShaped) direction
- Keep checking the inward (fromShaped) queues and if there's any data on it,
  send it to the respective client
- Whenever a client disconnects, inform the `ShapedSender` to send a FIN onwards
- If both end-hosts have sent a FIN, cleanup the assigned queues to be
  re-used later.

### Shaped Sender

This component is based on the module `QUIC Sender` and acts as a QUIC Client.
It does the following tasks:

- Connect to the other middlebox, establish a control, a dummy and multiple
  data streams (QUIC Streams)
- Whenever `UnshapedReceiver` informs about a new client joining, send a SYN
  on the control stream
- Periodically check the queue (toShaped) and make a DP decision, add that to
  `credit`
- Periodically send data/dummy based on the DP `credit` available
- If the other middlebox sends some data (e.g. response from the other host),
  push it to the relevant queue (fromShaped)
- If `UnshapedReceiver` informs about a client termination, send a FIN on
  the control stream
- If a FIN is received on the control stream, inform `UnshapedReceiver` about it

### Shaped Receiver

This component is based on the module `QUIC Receiver` and acts as a QUIC Server.
It does the following tasks:

- Listen for other middlebox connecting to it
- Assign a queue pair to each new client that it gets information about from
  the control stream
- Whenever data is received on the stream, push it to relevant (fromShaped)
  queue.
- Periodically check the queue (toShaped) and make a DP decision, add that to
  `credit`
- Periodically send data/dummy based on the DP `credit` available
- Forward FIN from either direction
- If both directions have sent FINs, cleanup queues to be re-used later

### Unshaped Sender

This component is based on the module `TCP Sender` and acts as a TCP Client.
It does the following tasks:

- Create a new connection to a server for each client that it gets
  information about from the `ShapedReceiver`
- Periodically read queue (fromShaped) and send data to the server
- Push data received from the server to the relevant queue (toShaped)
- Forward FIN in either direction
- Cleanup it's flow mappings whenever both directions have sent their FINs