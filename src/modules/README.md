# Modules

### tcp_wrapper

This module has 2 components:

1. **Server:** It is a TCP server, which listens on the specified
   port and calls a callback function `serverOnReceive` on every event outlined
   in
   [Common](#common). It also has a function called `sendData(...)` to send
   data to a given socket.
2. **Client:** It is a TCP client, which connect on the specified IP and port.
   You can send data using the `sendData` function or a FIN using the
   `sendFIN` function. It also has a callback function `serverOnReceive` which
   is
   triggered whenever it receives a response from the other side.

### quic_wrapper

This module has 2 components:

1. **Server:** It is a QUIC server, which listens on the specified
   port and calls a callback function `serverOnReceive` whenever data is
   received
   on a stream.

2. **Client:** It is a QUIC client, which connect on the specified IP and port.
   You can send data using the `send(...)` function . It also has a callback
   function `onReceive` which is triggered whenever it receives a response from
   the other side.

### lamport_queue

This module implements a lamport queue (which is a verified SCSP queue). It
provides the familiar `push` and `pop` functions.
The queue also stores some additional information (e.g. which client/server
this queue is currently assigned to)

### shaper

This is the heart of minesVPN. It's the module that accepts the queue sizes
and decides on the #bytes to be sent out such that the Differential Privacy
guarantees are met.

### Common

This header file contains some commonly used structs and enums:

- **logLevels:** `{ERROR, WARNING, DEBUG}`
- **connectionStatus:** `{SYN, ONGOING, FIN}`. Determines whether the
  callback function was called on a SYN/FIN or a regular data transmission
- **addressPair:** The structure which stores the client and server address
  and ports for various uses