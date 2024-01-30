#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'



# Maximum capture size of TCPdump (# bytes)
MAX_CAPTURE_SIZE=300 
# Length of captured traces (seconds) 
TIMEOUT=300 

# Number of parallel communication channels
MAX_PARALLEL=12


if [ -z $1 ]
then
  echo -e "${RED}Usage ./run_testbed.sh <Number of Iterations>${OFF}"
  exit 1
fi

cd ../../dataset/client/ || exit


# List of all files of type *.mpd
videoMPDs=($(find ./dataset -iname "*.mpd" -type f -print))

cd - || exit

# Remove traces from middleboxes

ssh desh03 "cd ~/workspace/minesvpn-testbed/experiments/macrobenchmarks/ && rm -r traces"
ssh desh02 "cd ~/workspace/minesvpn-testbed/experiments/macrobenchmarks/ && rm -r traces"


COUNTER=0
for ((i=1; i<=$1; i++)); do	
  echo -e "${CYAN}Running iteration $i${OFF}"
  for videoMPD in ${videoMPDs[@]}
  do
    videoMPD=$(echo ${videoMPD#./dataset/})
    # Run Peer2
    ssh desh03 "cd ~/workspace/minesvpn-testbed/experiments/macrobenchmarks/ && ./run_peer_2.sh $COUNTER $videoMPD $i $TIMEOUT"

    # Run Peer1
    ssh desh02 "cd ~/workspace/minesvpn-testbed/experiments/macrobenchmarks/ && ./run_peer_1.sh $COUNTER $videoMPD $i $TIMEOUT"

    #Run the video client
    echo -e "${YELLOW}Running the video client${OFF}"
    mkdir -p traces/Video-Bandwidth/Client/$i
    port=$((COUNTER + 8000))

    docker run \
      -d --rm \
      --name "video-client-$(basename $videoMPD .mpd)-$i" \
      -e VIDEO="$videoMPD" \
      -e PCAP="$(basename $videoMPD .mpd)" \
      -e TIMEOUT="$TIMEOUT" \
      -e SERVER="192.168.1.2:$port" \
      -e MAX_CAPTURE_SIZE="$MAX_CAPTURE_SIZE" \
      -v "$(pwd)/traces/Video-Bandwidth/Client/$i:/root/traces" \
      video-client

    COUNTER=$((COUNTER+1))
    if [[ $COUNTER -ge $MAX_PARALLEL ]]
    then
      echo Waiting for previous batch to finish
      sleep $((TIMEOUT+20))
      COUNTER=0
#      break
    fi
  done
done

echo -e "${YELLOW}Waiting for all container to stop${OFF}"
sleep $((TIMEOUT+20))

scp -r desh02:~/workspace/minesvpn-testbed/experiments/macrobenchmarks/traces/Video-Bandwidth/Peer1 ./traces/Video-Bandwidth/
scp -r desh02:~/workspace/minesvpn-testbed/experiments/macrobenchmarks/peer1-config.json ./traces/Video-Bandwidth/
scp -r desh03:~/workspace/minesvpn-testbed/experiments/macrobenchmarks/peer2-config.json ./traces/Video-Bandwidth/
scp -r desh03:~/workspace/minesvpn-testbed/experiments/macrobenchmarks/traces/Video-Bandwidth/Peer2 ./traces/Video-Bandwidth/

date=$(date +%Y-%m-%d_%H-%M)
mkdir -p data/$date
mv traces/Video-Bandwidth data/$date/

echo -e "${GREEN}Traces are stored in $(pwd)/traces${OFF}"