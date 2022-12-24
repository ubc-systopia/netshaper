#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'

# Build the proxy application
cmake -G Ninja -S $(pwd) -B $(pwd)/build
cmake --build $(pwd)/build -j $(nproc)

