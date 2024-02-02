#!/bin/bash

# This script is used for starting peer2.
# This script should be placed in the host containing peer2.
# This script can be executed via a terminal or ssh or another script.

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'



if [ -z $1 ] || [ -z $2 ] || [ -z $3 ] || [ -z $4 ] || [ -z $5 ]
then
  echo -e "${RED}Usage ./run.sh <Instance-Number> <MPD-File-Path> <Iteration-Number> <Timeout> <MAX_CAPTURE_SIZE>${OFF}"
  exit 1
fi

videoMPD=$2
i=$3
port=$((COUNTER + 8000))
TIMEOUT=$4 # Seconds

# Maximum capture size of TCPdump (# bytes)
MAX_CAPTURE_SIZE=$5

echo -e "${YELLOW}Running the video client${OFF}"
mkdir -p traces/client/$i

docker run \
  -d --rm \
  --name "video-client-$(basename $videoMPD .mpd)-$i" \
  -e VIDEO="$videoMPD" \
  -e PCAP="$(basename $videoMPD .mpd)" \
  -e TIMEOUT="$TIMEOUT" \
  -e SERVER="192.168.1.2:$port" \
  -e MAX_CAPTURE_SIZE="$MAX_CAPTURE_SIZE" \
  -v "$(pwd)/traces/client/$i:/root/traces" \
  video-client