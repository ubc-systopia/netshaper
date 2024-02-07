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
    echo "Usage: $0 --iter_num number --hostname_peer1 <host> --username_peer1 <user> --hostname_peer2 <host> --username_peer2 <user> --netshaper_dir_peer1 <dir> --netshaper_dir_peer2 <dir> --results_dir <dir>"
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
if [[ -z "$hostname_peer1" || -z "$username_peer1" || -z "$hostname_peer2" || -z "$username_peer2" || -z "$netshaper_dir_peer1" || -z "$netshaper_dir_peer2" || -z "$iter_num" || -z "$results_dir" ]]; then
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
ssh "$username_peer1@$hostname_peer1" "[[ -d \"$netshaper_dir_peer1/hardware/client-middlebox/traces\" ]] && rm -rf \"$netshaper_dir_peer1/hardware/client-middlebox/traces\""


# Remove traces from peer2 if the directory exists
ssh "$username_peer2@$hostname_peer2" "[[ -d \"$netshaper_dir_peer2/hardware/server-middlebox/traces\" ]] && rm -rf \"$netshaper_dir_peer2/hardware/server-middlebox/traces\""


# Remove traces from the video client if the directory exists
[[ -d "../../hardware/video-client/traces" ]] && rm -rf "../../hardware/video-client/traces"




COUNTER=0
for ((i=1; i<=$iter_num; i++)); do	
  echo -e "${CYAN}Running iteration $i${OFF}"
  for videoMPD in ${videoMPDs[@]}
  do


    # Run Peer2
    ssh "$username_peer2@$hostname_peer2" "cd $netshaper_dir_peer2/hardware/server-middlebox/ && ./run.sh $COUNTER $videoMPD $i $TIMEOUT $MAX_CAPTURE_SIZE"

    # Run Peer1
    ssh "$username_peer1@$hostname_peer1" "cd $netshaper_dir_peer1/hardware/client-middlebox/ && ./run.sh $COUNTER $videoMPD $i $TIMEOUT $MAX_CAPTURE_SIZE"


    # Run the video client
    cd ../../hardware/video-client/ || exit
    ./run.sh $COUNTER $videoMPD $i $TIMEOUT $MAX_CAPTURE_SIZE
    cd - >/dev/null 2>&1 || exit



    COUNTER=$((COUNTER+1))
    if [[ $COUNTER -ge $MAX_PARALLEL ]]
    then
      echo -e "${YELLOW}Waiting for $(($TIMEOUT+20)) seconds to finish the last batch of containers${OFF}"
      sleep $(($TIMEOUT+20))
      COUNTER=0
      break
    fi
  done
  break
done



echo -e "${YELLOW}Waiting for all container to stop${OFF}"
sleep $((TIMEOUT+20))

# Copy traces from peer1
mkdir -p "$results_dir/peer1/"
scp -r "$username_peer1@$hostname_peer1:$netshaper_dir_peer1/hardware/client-middlebox/traces" "$results_dir/peer1/"

# Copy traces from peer2
mkdir -p "$results_dir/peer2/"
scp -r "$username_peer2@$hostname_peer2:$netshaper_dir_peer2/hardware/server-middlebox/traces" "$results_dir/peer2/"

# Copy traces from the video client
mkdir -p "$results_dir/client/"
cp -r "../../hardware/video-client/traces" "$results_dir/client/"

echo -e "${GREEN}Traces are saved in $results_dir${OFF}"