#!/bin/bash

# This script is used for starting peer1.
# This script should be placed in the host containing peer1.
# This script can be executed via a terminal or ssh or another script.

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'

CONFIG=config.json # Path of the config file relative to /root in the container
MAX_PARALLEL=5

if [ -z $1 ] || [ -z $2 ] || [ -z $3 ] || [ -z $4 ] || [ -z $5 ]
then
  echo -e "${RED}Usage ./run.sh <Instance-Number> <MPD-File-Path> <Iteration-Number> <Timeout> <MAX_CAPTURE_SIZE>${OFF}"
  exit 1
fi

if (( $1 > 5 ))
then
  echo -e "${RED}Instance number should be less than 6${OFF}"
  exit 1
fi

videoMPD=$2
i=$3
peer_port=$(($1 + 4560))
port=$(($1 + 8000))
TIMEOUT=$4 # Seconds

# Maximum capture size of TCPdump (# bytes)
MAX_CAPTURE_SIZE=$5 



# Run Peer1
echo -e "${YELLOW}Starting Peer 1${OFF}"
mkdir -p traces/peer1/$i

docker run \
  -d --rm \
  -e TIMEOUT="$TIMEOUT" \
  -e MAX_CAPTURE_SIZE="$MAX_CAPTURE_SIZE" \
  -e PCAP="$(basename $videoMPD .mpd)" \
  -e CONFIG="$CONFIG" \
  -e PEER_PORT=$peer_port \
  --name "peer1-$(basename $videoMPD .mpd)-${i}" \
  -v "$(pwd)/traces/peer1/$i:/root/traces" \
  -v "$(pwd)/peer1_config.json:/root/config.json" \
  -p $port:8000 \
  peer1

echo -e "${YELLOW}Waiting for 5s${OFF}"
sleep 5

#echo -e "${GREEN}Traces are stored in $(pwd)/traces${OFF}"