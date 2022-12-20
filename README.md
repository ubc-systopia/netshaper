# TL;DR

Run the following commands in the root directory of this repository
#### Compile
1. `cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=ninja -G Ninja -S $(pwd) -B $(pwd)/build`
2. `cmake --build $(pwd)/build --target proxy_cpp -j $(nproc)`

#### Run
3. `./build/proxy_cpp` (Listens on port 8000)
Enter the IP address of the remote host and then the port of the remote host


