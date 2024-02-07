#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'

BUILD_TYPE=Release # Debug

# build wrk2 client
echo -e "${YELLOW}Building wrk2 client${OFF}"
cd wrk2
make clean 
make 