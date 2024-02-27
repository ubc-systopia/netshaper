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


#!/bin/bash

# Initialize variables
iter_num=""
peer1_ssh_host=""
peer1_ssh_username=""
peer2_ssh_host=""
peer2_ssh_username=""
peer1_netshaper_dir=""  
peer2_netshaper_dir=""
max_client_num=""
# Function to display usage information
usage() {
    echo "Usage: $0 --iter_num number --hostname_peer1 <host> --username_peer1 <user> --hostname_peer2 <host> --username_peer2 <user> --netshaper_dir_peer1 <dir> --netshaper_dir_peer2 <dir> --results_dir <dir> --max_client_num <num>"
    exit 1
}
# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --results_dir)
            results_dir="$2"
            shift 2
            ;;
        --iter_num)
            iter_num="$2"
            shift 2
            ;;
        --max_client_num)
            max_client_num="$2"
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
if [[ -z "$hostname_peer1" || -z "$username_peer1" || -z "$hostname_peer2" || -z "$username_peer2" || -z "$netshaper_dir_peer1" || -z "$netshaper_dir_peer2" || -z "$iter_num" || -z "$results_dir" || -z "$max_client_num" ]]; then
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
videoMPDs=($(cat ../../dataset/client/videos.txt))



cd - >/dev/null 2>&1 || exit



# Remove previous traces from middleboxes

# Remove traces from peer1 if the directory exists
ssh "$username_peer1@$hostname_peer1" "[[ -d \"$netshaper_dir_peer1/hardware/client_middlebox/traces\" ]] && rm -rf \"$netshaper_dir_peer1/hardware/client_middlebox/traces\""


# Remove traces from peer2 if the directory exists
ssh "$username_peer2@$hostname_peer2" "[[ -d \"$netshaper_dir_peer2/hardware/server_middlebox/traces\" ]] && rm -rf \"$netshaper_dir_peer2/hardware/server_middlebox/traces\""


# Remove traces from the video client if the directory exists
[[ -d "../../hardware/video_client/traces" ]] && rm -rf "../../hardware/video_client/traces"





instance_num=0
iter_num_middlebox=1

# Run Peer2
ssh "$username_peer2@$hostname_peer2" "cd $netshaper_dir_peer2/hardware/server_middlebox/ && ./run.sh $instance_num $videoMPD $iter_num_middlebox $TIMEOUT $MAX_CAPTURE_SIZE"

# Run Peer1
ssh "$username_peer1@$hostname_peer1" "cd $netshaper_dir_peer1/hardware/client_middlebox/ && ./run.sh $instance_num $videoMPD $iter_num_middlebox $TIMEOUT $MAX_CAPTURE_SIZE"



COUNTER=0
for ((i=1; i<=$iter_num; i++)); do	
  echo -e "${CYAN}Running iteration $i${OFF}"
  for videoMPD in ${videoMPDs[@]}
  do
    videoMPD="video/$videoMPD"


    # Run the video client
    cd ../../hardware/video_client/ || exit
    ./run.sh $COUNTER $videoMPD $i $TIMEOUT $MAX_CAPTURE_SIZE
    cd - >/dev/null 2>&1 || exit



    COUNTER=$((COUNTER+1))
    if [[ $COUNTER -ge $max_client_num ]]
    then
      break
    fi
  done
  # Break if the number of clients is reached
  if [[ $COUNTER -ge $max_client_num ]]
  then
    break
  fi
done



echo -e "${YELLOW}Waiting for all container to stop${OFF}"
sleep $((TIMEOUT+20))

# Copy traces from peer1
mkdir -p "$results_dir/peer1/"
scp -r "$username_peer1@$hostname_peer1:$netshaper_dir_peer1/hardware/client_middlebox/traces/peer1" "$results_dir/"

# Copy traces from peer2
mkdir -p "$results_dir/peer2/"
scp -r "$username_peer2@$hostname_peer2:$netshaper_dir_peer2/hardware/server_middlebox/traces/peer2" "$results_dir/"

# Copy traces from the video client
mkdir -p "$results_dir/client/"
cp -r "../../hardware/video_client/traces/client" "$results_dir/"

echo -e "${GREEN}Traces are saved in $results_dir${OFF}"