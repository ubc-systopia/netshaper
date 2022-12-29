# TL;DR

Run the following commands in the root directory of this repository

#### Prerequisites (Commands are for ubuntu)
0. Clone this repository with `--recurse-submodules` option
1. `./setup.sh` (Run with `sudo` or as root user)

#### Compile
2. `./build.sh`

#### Run

##### UnshapedTransceiever Example (Runs as a simple proxy that can handle one client at a time)  
3. `./build/bin/UnshapedTransceiver/unshapedTransciever` (Listens on port 8000)  
Enter the IP address of the remote host and then the port of the remote host

##### ShapedTransciever Example (Can run either as a client or a server)
3. `./build/bin/ShapedTransciever/shapedTransciever <option>`  
  Options:  
  `-c` (for client)  
  `-s` (for server)  
_Note: The example expects both to run on the same device (i.e. client connects on localhost)_
