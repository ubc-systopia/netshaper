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

peer1_IP="192.168.1.2"

# NetShaper directory at server-side middlebox (peer2)
peer2_netshaper_dir="/home/minesvpn/workspace/artifact_evaluation/code/minesvpn"


# NetShaper directory at client-side middlebox (peer1)
peer1_netshaper_dir="/home/minesvpn/workspace/artifact_evaluation/code/minesvpn"


# Config directory
config_file_dir="$(pwd)/configs/microbenchmarks_small_requests.json"


# Results directory
date=$(date +%Y-%m-%d_%H-%M)

results_dir="$(pwd)/results/microbenchmark_($date)/"



max_client_numbers=($(jq -r '.max_client_numbers[]' $config_file_dir))

iter_num=($(jq -r '.iter_num' $config_file_dir))
request_rate=($(jq -r '.request_rate' $config_file_dir))
request_size=($(jq -r '.request_size' $config_file_dir))


for max_client_number in "${max_client_numbers[@]}"; do

  echo -e "${YELLOW}Modifying the config file in peer1 and Peer2${OFF}"
  python3 helpers/set_params.py --max_client_num $max_client_number --hostname_peer1 $peer1_ssh_host --username_peer1 $peer1_ssh_username --hostname_peer2 $peer2_ssh_host --username_peer2 $peer2_ssh_username --netshaper_dir_peer1 $peer1_netshaper_dir --netshaper_dir_peer2 $peer2_netshaper_dir --experiment_config_path $config_file_dir

  experiment_results_dir="$results_dir/C_$max_client_number"


  echo -e "${YELLOW}Running experiment with max client number = $max_client_number$.{OFF}"
  ./helpers/run_testbed.sh --hostname_peer1 $peer1_ssh_host --username_peer1 $peer1_ssh_username --IP_peer1 $peer1_IP --hostname_peer2 $peer2_ssh_host --username_peer2 $peer2_ssh_username --netshaper_dir_peer1 $peer1_netshaper_dir --netshaper_dir_peer2 $peer2_netshaper_dir --iter_num $iter_num --results_dir $experiment_results_dir --max_client_num $max_client_number --request_rate $request_rate --request_size $request_size

  sleep 5
done

echo -e "${GREEN}All experiments are done${OFF}"

echo -e "${YELLOW}Processing the data...${OFF}"
python3 helpers/process_data.py --results_dir $results_dir


echo -e "${YELLOW}Plotting the data...${OFF}"
python3 helpers/plot_data.py --results_dir $results_dir


echo -e "${GREEN}All experiments are done${OFF}"


