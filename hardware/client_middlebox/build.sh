#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'

BUILD_TYPE=Debug # Debug

# Build the proxy application
cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -G Ninja -S $(pwd) -B $(pwd)/peer1/build
cmake --build $(pwd)/peer1/build -j $(nproc)



