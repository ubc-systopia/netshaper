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
MAX_PARALLEL=6



# Function to display usage information
usage() {
    echo "Usage: $0 --iter_num number --max_client_num number --request_rate number --hostname_peer1 <host> --username_peer1 <user> --IP_peer1 <IP> --hostname_peer2 <host> --username_peer2 <user> --netshaper_dir_peer1 <dir> --netshaper_dir_peer2 <dir> --results_dir <dir>"
    exit 1
}
# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --results_dir)
            results_dir="$2"
            shift 2
            ;;
        --IP_peer1)
            IP_peer1="$2"
            shift 2
            ;;
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
        --max_client_num)
            max_client_num="$2"
            shift 2
            ;;
        --request_rate)
            request_rate="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# Check if all required arguments are provided
if [[ -z "$hostname_peer1" || -z "$username_peer1" || -z "$hostname_peer2" || -z "$username_peer2" || -z "$netshaper_dir_peer1" || -z "$netshaper_dir_peer2" || -z "$iter_num" || -z "$results_dir" || -z "$max_client_num" || -z "$request_rate" || -z "$IP_peer1" ]]; then
    echo "Missing required arguments."
    usage
fi

# Dummy request size as this experiment is not for microbenchmarks
request_size=0

# Remove stats from the web client if the directory exists
[[ -d "../../hardware/web_client/latencies" ]] && rm -rf "../../hardware/web_client/latencies"


for ((i=1; i<=$iter_num; i++)); do	

  echo -e "${CYAN}Running iteration $i${OFF}"

  PCAP_FILE="iter_$i.mpd"
  # Run Peer2
  ssh "$username_peer2@$hostname_peer2" "cd $netshaper_dir_peer2/hardware/server_middlebox/ && ./run.sh $i $PCAP_FILE $i $TIMEOUT $MAX_CAPTURE_SIZE"

  # Run Peer1
  ssh "$username_peer1@$hostname_peer1" "cd $netshaper_dir_peer1/hardware/client_middlebox/ && ./run.sh $i $PCAP_FILE $i $TIMEOUT $MAX_CAPTURE_SIZE"


  # Run the web client
  cd ../../hardware/web_client/ || exit
  ./run.sh "web-latency" $max_client_num $request_rate $request_size $i $IP_peer1
  cd - > /dev/null 2>&1 || exit


  sleep $(($TIMEOUT+20))
done



echo -e "${YELLOW}Waiting for all container to stop${OFF}"
sleep $((TIMEOUT+20))


# Copy traces from the video client
mkdir -p "$results_dir/client/"
cp -r "../../hardware/web_client/latencies" "$results_dir/client/"

echo -e "${GREEN}Traces are saved in $results_dir${OFF}"



