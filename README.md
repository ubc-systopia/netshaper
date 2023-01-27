# TL;DR

Run the following commands in the root directory of this repository

## Prerequisites (Commands are for ubuntu)

0. Clone this repository with `--recurse-submodules` option
1. `./setup.sh` (Run with `sudo` or as root user)

## Compile

2. `./build.sh`

## Run

### End-to-end example

_Note: Currently the actual server is hard-coded to be at "127.0.0.1:5555" in_ `src/Example/Peer1/unshaped.cpp`  
_Note 2: Currently, the minesVPN peer1 listens on port 8000, where the actual client should connect_

#### Automated test script

3. `./test.sh`

#### Run Individual components
3. First start the actual server (note that currently, the actual server is expected to be at 127.0.0.1:5555 on the same node as peer2)
4. Next, start the unshaped processes on both peers
   - For peer 2: `./build/src/Example/Peer2/peer_2_unshaped` and enter max number of queues to initialise
   - For Peer 1: `./build/src/Example/Peer1/peer_1_unshaped` and enter max number of queues to initialise
5. Then start shaped process on peer 2 (the one in front of the server)
   - Generate a self-signed certificate using: `openssl req -nodes -new -x509 -keyout ./build/src/Example/Peer2/server.key -out ./build/src/Example/Peer2/server.cert`
   - `./build/src/Example/Peer2/peer_2_shaped` and enter the max number of queues to initialise (should be same as the number of queues stated in peer_2_unshaped)
6. Finally, start the shaped process on peer 1 (the one in front of the client)
   - `./build/src/Example/Peer1/peer_1_shaped` and enter the max number of queues to initialise (should be same as the number of queues stated in peer_1_unshaped)
7. Now, you can send requests from any client to peer1's port 8000

### UnshapedTransceiever Example

_(Runs as a simple proxy that can handle one client at a time)_

3. `./build/UnshapedTransceiver/unshapedTransciever` (Listens on port 8000)  
   Enter the IP address of the remote host and then the port of the remote host

### ShapedTransciever Example

_(Can run either as a client or a server)_

3. Generate a self-signed certificate
   using `openssl req -nodes -new -x509 -keyout ./build/ShapedTransciever/server.key -out ./build/ShapedTransciever/server.cert`
4. `./build/ShapedTransciever/shapedTransciever <option>`  
   Options:  
   `-c` (for client)  
   `-s` (for server)  
   _Note: The example expects both to run on the same device (i.e. client
   connects on localhost)_
