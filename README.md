# TL;DR

Run the following commands in the root directory of this repository

#### Prerequisites (Commands are for ubuntu)
0. Clone this repository with `--recurse-submodules` option
1. `./setup.sh` (Run with `sudo` or as root user)

#### Compile
2. `./build.sh` (Expects you to have libssl.so.1.1 and libcrypto.so.1.1)  

#### Run

##### UnshapedTransceiever Example
_(Runs as a simple proxy that can handle one client at a time)_  
3. `./build/bin/UnshapedTransceiver/unshapedTransciever` (Listens on port 8000)  
Enter the IP address of the remote host and then the port of the remote host

##### ShapedTransciever Example
_(Can run either as a client or a server)_  
3. Generate a self-signed certificate using `openssl req  -nodes -new -x509  -keyout ./build/bin/ShapedTransciever/server.key -out ./build/bin/ShapedTransciever/server.cert`  
4. `./build/bin/ShapedTransciever/shapedTransciever <option>`  
  Options:  
  `-c` (for client)  
  `-s` (for server)  
_Note: The example expects both to run on the same device (i.e. client connects on localhost)_
