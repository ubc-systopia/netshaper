# TL;DR

Run the following commands in the root directory of this repository

#### Prerequisites (Commands are for ubuntu)
0. Clone this repository with `--recurse-submodules` option
1. `./setup.sh` (Run with `sudo` or as root user)

#### Compile
2. `./build.sh` (Expects you to have libssl.so.1.1 and libcrypto.so.1.1)  

#### Run

##### End-to-end example
_Note: This example does not do any shaping currently. Just forwards data in the following path:_
`client--->middlebox1--->middlebox2--->server` (note that it is unidirectional)  
Open a new terminal for each device (if testing on localhost only)  
  
3. Terminal 1: `nc -l -p 5555` (Acts as a simple tcp server)
4. Terminal 2: `./build/example_peer_2` (Middlebox on the server side)  
Enter `127.0.0.1` followed by `5555` (the netcat server we initialised 
   in Terminal 1)
5. Terminal 3: `./build/example_peer_1` (Middlebox on client side)  
6. Terminal 4: `nc localhost 8000` (The client)  
7. Type anything in the 4th terminal and press 'Enter', it will appear in the 1st terminal

##### UnshapedTransceiever Example
_(Runs as a simple proxy that can handle one client at a time)_  
3. `./build/UnshapedTransceiver/unshapedTransciever` (Listens on port 8000)  
Enter the IP address of the remote host and then the port of the remote host

##### ShapedTransciever Example
_(Can run either as a client or a server)_  
3. Generate a self-signed certificate using `openssl req  -nodes -new -x509  -keyout ./build/ShapedTransciever/server.key -out ./build/ShapedTransciever/server.cert`  
4. `./build/ShapedTransciever/shapedTransciever <option>`  
  Options:  
  `-c` (for client)  
  `-s` (for server)  
_Note: The example expects both to run on the same device (i.e. client connects on localhost)_
