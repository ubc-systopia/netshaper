#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'





# SSH Host and Username for server-side middlebox (peer2)
peer2_ssh_host="desh03"
peer2_ssh_username="minesvpn"

# SSH Host and Username for client-side middlebox (peer1)
peer1_ssh_host="desh02"
peer1_ssh_username="minesvpn"

# TODO: this line should be read from the json file. 
dp_intervals_peer2=($(jq -r '.dp_intervals_peer2[]' configs/video_latency.json))
iter_num=($(jq -r '.iter_num' configs/video_latency.json))



for dp_interval_peer2 in "${dp_intervals_peer2[@]}"; do
  echo -e "${YELLOW}Modifying the config file in peer2${OFF}"

  python3 helpers/set_params.py --dp_interval_peer2 $dp_interval_peer2 --hostname_peer1 $peer1_ssh_host --username_peer1 $peer1_ssh_username --hostname_peer2 $peer2_ssh_host --username_peer2 $peer2_ssh_username

  echo -e "${YELLOW}Running experiment with peer2_DP_interval = $dp_interval_peer2...${OFF}"
  ./run_testbed.sh $iter_num   

  sleep 5
done


# for peer2_DP_interval in "${peer2_DP_intervals[@]}"
# #for peer2_noise_multiplier in "${peer2_noise_multipliers[@]}"
# do
#   echo -e "${YELLOW}Running experiment with peer2_DP_interval = $peer2_DP_interval...${OFF}"
#   ./change_params_and_run.sh $peer2_DP_interval
#   #./change_params_and_run.sh $peer2_noise_multiplier
#   echo -e "${GREEN}Experiment is finished.${OFF}"
#   echo -e "${YELLOW}Waiting for 5 seconds...${OFF}"
#   sleep 5
#   # change reults to csv
#   echo -e "${YELLOW}Changing results to csv...${OFF}"
#   ./helpers/pcap_to_csvs.sh
#   echo -e "${GREEN}Results are changed to csv.${OFF}"
#   echo -e "${YELLOW}Waiting for 5 seconds...${OFF}"
#   sleep 5
#   # parse and save logs
#   echo -e "${YELLOW}Parsing and saving logs...${OFF}"
#   ./helpers/parsing_logs.sh
#   echo -e "${GREEN}Logs are parsed and saved.${OFF}"
#   echo -e "${YELLOW}Waiting for 5 seconds...${OFF}"
#   sleep 500
# done





# if [ -z $1 ]
# then
#   echo -e "${RED}Usage ./run_parallel_experiments.sh <Iterations>${OFF}"
#   exit 1
# fi

# cd ../../host_2/nginx_server || exit

# # List of all files of type *.mpd
# videoMPDs=($(find ./dataset -iname "*.mpd" -type f -print))

# cd - || exit

# # Remove traces from middleboxes
# ssh desh03 "cd ~/workspace/minesvpn-testbed/experiments/macrobenchmarks/ && rm -r traces"
# ssh desh02 "cd ~/workspace/minesvpn-testbed/experiments/macrobenchmarks/ && rm -r traces"


# COUNTER=0
# for ((i=1; i<=$1; i++)); do	
#   echo -e "${CYAN}Running iteration $i${OFF}"
#   for videoMPD in ${videoMPDs[@]}
#   do
#     videoMPD=$(echo ${videoMPD#./dataset/})
#     # Run Peer2
#     ssh desh03 "cd ~/workspace/minesvpn-testbed/experiments/macrobenchmarks/ && ./run_peer_2.sh $COUNTER $videoMPD $i $TIMEOUT"

#     # Run Peer1
#     ssh desh02 "cd ~/workspace/minesvpn-testbed/experiments/macrobenchmarks/ && ./run_peer_1.sh $COUNTER $videoMPD $i $TIMEOUT"

#     #Run the video client
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
#   done
# done

# echo -e "${YELLOW}Waiting for all container to stop${OFF}"
# sleep $((TIMEOUT+20))

# scp -r desh02:~/workspace/minesvpn-testbed/experiments/macrobenchmarks/traces/Video-Bandwidth/Peer1 ./traces/Video-Bandwidth/
# scp -r desh02:~/workspace/minesvpn-testbed/experiments/macrobenchmarks/peer1-config.json ./traces/Video-Bandwidth/
# scp -r desh03:~/workspace/minesvpn-testbed/experiments/macrobenchmarks/peer2-config.json ./traces/Video-Bandwidth/
# scp -r desh03:~/workspace/minesvpn-testbed/experiments/macrobenchmarks/traces/Video-Bandwidth/Peer2 ./traces/Video-Bandwidth/

# date=$(date +%Y-%m-%d_%H-%M)
# mkdir -p data/$date
# mv traces/Video-Bandwidth data/$date/

# echo -e "${GREEN}Traces are stored in $(pwd)/traces${OFF}"