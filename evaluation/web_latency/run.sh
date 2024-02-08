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


# NetShaper directory at server-side middlebox (peer2)
peer2_netshaper_dir="/home/minesvpn/workspace/artifact_evaluation/code/minesvpn"


# NetShaper directory at client-side middlebox (peer1)
peer1_netshaper_dir="/home/minesvpn/workspace/artifact_evaluation/code/minesvpn"


# Results directory
date=$(date +%Y-%m-%d_%H-%M)

results_dir="$(pwd)/results/web_latency_($date)/"



dp_intervals_peer2=($(jq -r '.dp_intervals_peer2[]' configs/web_latency.json))

iter_num=($(jq -r '.iter_num' configs/web_latency.json))



for dp_interval_peer2 in "${dp_intervals_peer2[@]}"; do

  echo -e "${YELLOW}Modifying the config file in peer1 and Peer2${OFF}"

  python3 helpers/set_params.py --dp_interval_peer2 $dp_interval_peer2 --hostname_peer1 $peer1_ssh_host --username_peer1 $peer1_ssh_username --hostname_peer2 $peer2_ssh_host --username_peer2 $peer2_ssh_username --netshaper_dir_peer1 $peer1_netshaper_dir --netshaper_dir_peer2 $peer2_netshaper_dir

  experiment_results_dir="$results_dir/T_$dp_interval_peer2"


  echo -e "${YELLOW}Running experiment with peer2_DP_interval = $dp_interval_peer2...${OFF}"
  ./helpers/run_testbed.sh --hostname_peer1 $peer1_ssh_host --username_peer1 $peer1_ssh_username --hostname_peer2 $peer2_ssh_host --username_peer2 $peer2_ssh_username --netshaper_dir_peer1 $peer1_netshaper_dir --netshaper_dir_peer2 $peer2_netshaper_dir --iter_num $iter_num --results_dir $experiment_results_dir  

  sleep 5
done

# echo -e "${GREEN}All experiments are done${OFF}"



