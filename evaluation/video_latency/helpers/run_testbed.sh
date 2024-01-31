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

#!/bin/bash

# Initialize variables
iter_num=""
peer1_ssh_host=""
peer1_ssh_username=""
peer2_ssh_host=""
peer2_ssh_username=""
peer1_netshaper_dir=""  
peer2_netshaper_dir=""

# Function to display usage information
usage() {
    echo "Usage: $0 --iter_num number --hostname_peer1 <host> --username_peer1 <user> --hostname_peer2 <host> --username_peer2 <user> --netshaper_dir_peer1 <dir> --netshaper_dir_peer2 <dir>"
    exit 1
}
# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --iter_num)
            iter_num="$2"
            shift 2
            ;;
        --hostname_peer1)
            hostname_peer1="$2"
            shift 2
            ;;
        --username_peer1)
            username_peer1="$2"
            shift 2
            ;;
        --hostname_peer2)
            hostname_peer2="$2"
            shift 2
            ;;
        --username_peer2)
            username_peer2="$2"
            shift 2
            ;;
        --netshaper_dir_peer1)
            netshaper_dir_peer1="$2"
            shift 2
            ;;
        --netshaper_dir_peer2)
            netshaper_dir_peer2="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# Check if all required arguments are provided
if [[ -z "$hostname_peer1" || -z "$username_peer1" || -z "$hostname_peer2" || -z "$username_peer2" || -z "$netshaper_dir_peer1" || -z "$netshaper_dir_peer2" || -z "$iter_num" ]]; then
    echo "Missing required arguments."
    usage
fi

# # Use the variables as needed
# echo "Peer 1 SSH Host: $peer1_ssh_host"
# echo "Peer 1 SSH Username: $peer1_ssh_username"
# echo "Peer 2 SSH Host: $peer2_ssh_host"
# echo "Peer 2 SSH Username: $peer2_ssh_username"
# echo $iter_num


cd ../../dataset/client/ || exit

# List of all files of type *.mpd
videoMPDs=($(find ../../dataset/client/ -iname "*.mpd" -type f -print))



cd - || exit


# Remove previous traces from middleboxes

# Remove traces from peer1
ssh "$username_peer1@$hostname_peer1" "cd $netshaper_dir_peer1/hardware/client-middlebox/ && rm -r data/traces"


# Remove traces from peer2
ssh "$username_peer2@$hostname_peer2" "cd $netshaper_dir_peer2/hardware/server-middlebox/ && rm -r data/traces"


# 




COUNTER=0
for ((i=1; i<=$iter_num; i++)); do	
  echo -e "${CYAN}Running iteration $i${OFF}"
  for videoMPD in ${videoMPDs[@]}
  do
    videoMPD=$(echo ${videoMPD#*client/})


    # Run Peer2
    ssh "$username_peer2@$hostname_peer2" "cd $netshaper_dir_peer2/hardware/server-middlebox/ && ./run_peer2.sh $COUNTER $videoMPD $i $TIMEOUT"

    # Run Peer1
    ssh "$username_peer1@$hostname_peer1" "cd $netshaper_dir_peer1/hardware/client-middlebox/ && ./run_peer1.sh $COUNTER $videoMPD $i $TIMEOUT"


    #Run the video client
#     echo -e "${YELLOW}Running the video client${OFF}"
#     mkdir -p traces/Video-Bandwidth/Client/$i
#     port=$((COUNTER + 8000))

#     docker run \
#       -d --rm \
#       --name "video-client-$(basename $videoMPD .mpd)-$i" \
#       -e VIDEO="$videoMPD" \
#       -e PCAP="$(basename $videoMPD .mpd)" \
#       -e TIMEOUT="$TIMEOUT" \
#       -e SERVER="192.168.1.2:$port" \
#       -e MAX_CAPTURE_SIZE="$MAX_CAPTURE_SIZE" \
#       -v "$(pwd)/traces/Video-Bandwidth/Client/$i:/root/traces" \
#       video-client

#     COUNTER=$((COUNTER+1))
#     if [[ $COUNTER -ge $MAX_PARALLEL ]]
#     then
#       echo Waiting for previous batch to finish
#       sleep $((TIMEOUT+20))
#       COUNTER=0
# #      break
#     fi
  done
done
















# # echo -e "${YELLOW}Waiting for all container to stop${OFF}"
# # sleep $((TIMEOUT+20))

# # scp -r desh02:~/workspace/minesvpn-testbed/experiments/macrobenchmarks/traces/Video-Bandwidth/Peer1 ./traces/Video-Bandwidth/
# # scp -r desh02:~/workspace/minesvpn-testbed/experiments/macrobenchmarks/peer1-config.json ./traces/Video-Bandwidth/
# # scp -r desh03:~/workspace/minesvpn-testbed/experiments/macrobenchmarks/peer2-config.json ./traces/Video-Bandwidth/
# # scp -r desh03:~/workspace/minesvpn-testbed/experiments/macrobenchmarks/traces/Video-Bandwidth/Peer2 ./traces/Video-Bandwidth/

# # date=$(date +%Y-%m-%d_%H-%M)
# # mkdir -p data/$date
# # mv traces/Video-Bandwidth data/$date/

# # echo -e "${GREEN}Traces are stored in $(pwd)/traces${OFF}"