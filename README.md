# TL;DR

Run the following commands in the root directory of this repository

#### Prerequisites (Commands are for ubuntu)

0. Clone this repository with `--recurse-submodules` option
1. `./setup.sh` (Run with `sudo` or as root user)

#### Compile

2. `./build.sh`

#### Run

##### End-to-end example

_Note: It is unidirectional only  
Open 6 new terminal instances (if testing on localhost)

3. Generate a self-signed certificate
   using `openssl req -nodes -new -x509 -keyout ./build/server.key -out ./build/server.cert`

4. Terminal 1: `nc -l -p 5555` (Acts as a simple tcp server)
5. Terminal 2: `./build/example_peer_2_unshaped` (Unshaped Sender component of
   the middlebox on the
   server
   side)  
   Enter `127.0.0.1` followed by `5555` (the netcat server we initialised
   in Terminal 1)
6. Terminal 3: `./build/example_peer_2_shaped` (Shaped Receiver
   component in the middlebox on client side)
7. Terminal 4: `./build/example_peer_1_unshaped` (Unshaped Receiver in the
   middlebox on the client side)  
   _Enter the number of max clients you want to support_
8. Terminal 5: `./build/example_peer_1_shaped` (Shaped Sender in the
   middlebox on the client side)  
   _Enter the number of max clients you want to support (same as in Terminal 5)_
9. Terminal 6: `nc localhost 8000` (The client)
10. Type anything in the 4th terminal and press 'Enter', it will appear in
    the 1st terminal, alongside dummy data (currently set to multiple 'a's)

##### UnshapedTransceiever Example

_(Runs as a simple proxy that can handle one client at a time)_

3. `./build/UnshapedTransceiver/unshapedTransciever` (Listens on port 8000)  
   Enter the IP address of the remote host and then the port of the remote host

##### ShapedTransciever Example

_(Can run either as a client or a server)_

3. Generate a self-signed certificate
   using `openssl req -nodes -new -x509 -keyout ./build/ShapedTransciever/server.key -out ./build/ShapedTransciever/server.cert`
4. `./build/ShapedTransciever/shapedTransciever <option>`  
   Options:  
   `-c` (for client)  
   `-s` (for server)  
   _Note: The example expects both to run on the same device (i.e. client
   connects on localhost)_
