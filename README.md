# TL;DR

Run the following commands in the root directory of this repository  
_Note: Currently minesVPN supports only 1 pair of middleboxes per program
instance_ (each peer can talk to only 1 other peer)

## Prerequisites (Commands are for ubuntu)

0. Clone this repository with `--recurse-submodules` option
1. `./setup.sh` (Run with `sudo` or as root user)

## Compile

2. `./build.sh`

## Run

### End-to-end example

#### Run Individual components

3. First start the actual server (or ensure it is up)
4. Start Peer 2 using `./build/src/example/peer2/peer_2` and enter the path to
   the config file. Wait for the "Peer is ready" message to appear
5. Start Peer 1 using `./build/src/example/peer1/peer_1` and enter the path to
   the config file. Wait for the   "Peer is ready" message to appear
7. Now, you can send requests from any client to peer1

### UnshapedTransceiever Example

_(Runs as a simple proxy that can handle one client at a time)_

3. `./build/src/modules/tcp_wrapper/exampleTCPWrapper` (Listens on port 8000)  
   Enter the IP address of the remote host and then the port of the remote host

### ShapedTransciever Example

_(Can run either as a client or a server)_

3. Generate a self-signed certificate
   using `openssl req -nodes -new -x509 -keyout ./build/src/modules/quic_wrapper/server.key -out ./build/src/modules/quic_wrapper/server.cert`
4. `./build/src/modules/quic_wrapper/exampleQUICWrapper <option>`  
   Options:  
   `-c` (for client)  
   `-s` (for server)  
   _Note: The example expects both to run on the same device (i.e. client
   connects on localhost)_

# Configuration

### Peer 1

Peer 1 can be configured with the following parameters provided in a json
file.  
The parameters shown here are the default values that will be used if a
parameter is not present in the config file

```json
{
  "logLevel": "DEBUG",
  "maxClients": 40,
  "shapedSender": {
    "peer2Addr": "localhost",
    "peer2Port": 4567,
    "noiseMultiplier": 38,
    "sensitivity": 500000,
    "maxDecisionSize": 500000,
    "minDecisionSize": 0,
    "DPCreditorLoopInterval": 50000,
    "senderLoopInterval": 50000,
    "sendingStrategy": "BURST"
  },
  "unshapedReceiver": {
    "bindAddr": "",
    "bindPort": 8000,
    "checkResponseLoopInterval": 50000,
    "serverAddr": "localhost:5555"
  }
}
```

#### General

- `logLevel` can be one of `DEBUG`, `WARNING`, `ERROR` only. Any other value
  will result in an error.
- `maxClients` is the maximum number of clients this system
  will support. The system will initialise exactly `maxClient` number of queue
  pairs.
- `shapedSender` is a json object containing the parameters to configure the
  shapedSender component
- `unshapedReceiver` is a json object containing the parameters to configure the
  unshapedReceiver component

#### shapedSender

- `peer2Addr` is the address where Peer 2 is hosted
- `peer2Port` is the port which Peer 2 is listening on (QUIC Listener)
- `noiseMultiplier` and `sensitivity` are the Differential Privacy parameters
- `minDecisionSize` and `maxDecisionSize` are the minimum and maximum
  decision size that the system should give out (we curb the maximum to
  ensure we don't get a decision of "infinite")
- `DPCreditorLoopInterval` is the time interval in microseconds with which the
  loop that reads the queue and adds to the "sending credit" should be run  
  _Note: This should be a multiple of `senderLoopInterval`_
- `senderLoopInterval` is the time interval in microseconds with which the
  loop that reads the "sending credit" and sends the data to Peer 2 should
  be run
- `sendingStrategy` defines how to send the data when the DP Creditor loop
  is run at a higher interval than the sending loop. Currently, the values it
  accepts are "BURST" (send all data ASAP) and "UNIFORM" (divide the credit
  equally across all intervals till the next decision time)

#### unshapedReceiver

- `bindAddr` is the address peer 1's TCP listener will listen on (for
  unshaped traffic to be proxy-ied via minesVPN). The default is "", which
  is equivalent to "::0" or "0.0.0.0"
- `bindPort` is the port that peer 1 will listen to
- `checkResponseLoopInterval` is the interval with which the queues will be
  checked for responses from the shaped component
- `serverAddr` is the address of the actual server that client is attempting
  to reach. As minesVPN currently does NOT do MITM Proxy, we resort to the
  assumption that all clients want to communicate to one server, mentioned here.

### Peer 2

Peer 2 can be configured with the following parameters provided in a json
file.  
The parameters shown here are the default values that will be used if a
parameter is not present in the config file

```json
{
  "logLevel": "DEBUG",
  "maxStreamsPerPeer": 40,
  "shapedReceiver": {
    "serverCert": "server.cert",
    "serverKey": "server.key",
    "listeningPort": 4567,
    "noiseMultiplier": 38,
    "sensitivity": 500000,
    "maxDecisionSize": 500000,
    "minDecisionSize": 0,
    "DPCreditorLoopInterval": 50000,
    "senderLoopInterval": 50000,
    "sendingStrategy": "BURST"
  },
  "unshapedSender": {
    "checkQueuesInterval": 50000
  }
}
```

#### General

- `logLevel` can be one of `DEBUG`, `WARNING`, `ERROR` only. Any other value
  will result in an error.
- `maxStreamsPerPeer` is the maximum number of streams this
  system will support per peer. As minesVPN currently supports only 1 peer
  at a time, the system will initialise exactly  `maxStreamsPerPeer`  number of
  queue pairs.
- `shapedSender` is a json object containing the parameters to configure the
  shapedSender component
- `unshapedReceiver` is a json object containing the parameters to configure the
  unshapedReceiver component

#### shapedReceiver

- `serverCert` is the path to the server certificate file
- `serverKey` is the path to the server key file  
  _Note: Both can be generated
  using `openssl req -nodes -new -x509 -keyout server.key -out server.cert`_

- `noiseMultiplier` and `sensitivity` are the Differential Privacy parameters
- `minDecisionSize` and `maxDecisionSize` are the minimum and maximum
  decision size that the system should give out (we curb the maximum to
  ensure we don't get a decision of "infinite")
- `DPCreditorLoopInterval` is the time interval in microseconds with which the
  loop that reads the queue and adds to the "sending credit" should be run  
  _Note: This should be a multiple of `senderLoopInterval`_
- `senderLoopInterval` is the time interval in microseconds with which the
  loop that reads the "sending credit" and sends the data to Peer 2 should
  be run
- `sendingStrategy` defines how to send the data when the DP Creditor loop
  is run at a higher interval than the sending loop. Currently, the values it
  accepts are "BURST" (send all data ASAP) and "UNIFORM" (divide the credit
  equally across all intervals till the next decision time)

#### unshapedReceiver

- `checkQueuesInterval` is the interval with which the queues will be
  checked for responses from the shaped component