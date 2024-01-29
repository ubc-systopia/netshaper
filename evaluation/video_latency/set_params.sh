#!/bin/bash
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'


# TODO: this should change according to setup that they use.
SSH_USER_PEER1=minesvpn@desh02
SSH_USER_PEER2=minesvpn@desh03


# TODO: Pathes are either should be relative to the home directory or should be manually written.
PEER1_JSON_FILE_PATH="/home/minesvpn/workspace/minesvpn-testbed/experiments/macrobenchmarks/peer1-configs.json"
PEER2_JSON_FILE_PATH="/home/minesvpn/workspace/minesvpn-testbed/experiments/macrobenchmarks/peer2-configs.json"


iter_num=$(jq -r '.ITER_NUM' configs/video_latency.json)
updated_noise_multiplier_peer1=$(jq -r '.noise_multiplier_peer1' configs/video_latency.json)
updated_noise_multiplier_peer2=$(jq -r '.noise_multiplier_peer2' configs/video_latency.json)
updated_sensitivity_peer1=$(jq -r '.sensitivity_peer1' configs/video_latency.json)
updated_sensitivity_peer2=$(jq -r '.sensitivity_peer2' configs/video_latency.json)
updated_dp_interval_peer1=$(jq -r '.dp_interval_peer1' configs/video_latency.json)
updated_sender_loop_interval_peer1=$(jq -r '.sender_loop_interval_peer1' configs/video_latency.json)
updated_dp_interval_peer2=$(jq -r '.dp_interval_peer2' configs/video_latency.json)
updated_sender_loop_interval_peer2=$(jq -r '.sender_loop_interval_peer2' configs/video_latency.json)
updated_max_decision_size_peer1=$(jq -r '.max_decision_size_peer1' configs/video_latency.json)
updated_min_decision_size_peer1=$(jq -r '.min_decision_size_peer1' configs/video_latency.json)
updated_max_decision_size_peer2=$(jq -r '.max_decision_size_peer2' configs/video_latency.json)
updated_min_decision_size_peer2=$(jq -r '.min_decision_size_peer2' configs/video_latency.json)



# Update peer1 JSON file
echo -e "${YELLOW}Connecting to $SSH_USER_PEER1...${OFF}"
status=$(ssh -o BatchMode=yes -o ConnectTimeout=5 "$SSH_USER_PEER1" echo ok 2>&1)
if [[ $status == ok ]] ; then
  echo -e "${GREEN}Connection was successful.${OFF}"
  ssh "$SSH_USER_PEER1" "jq --argjson updated_noise_multiplier_peer1 \"$updated_noise_multiplier_peer1\" \
                                --argjson updated_sensitivity_peer1 \"$updated_sensitivity_peer1\" \
                                --argjson updated_dp_interval_peer1 \"$updated_dp_interval_peer1\" \
                                --argjson updated_sender_loop_interval_peer1 \"$updated_sender_loop_interval_peer1\" \
                                --argjson updated_max_decision_size_peer1 \"$updated_max_decision_size_peer1\" \
                                --argjson updated_min_decision_size_peer1 \"$updated_min_decision_size_peer1\" \
                                '.shapedSender |= (.noiseMultiplier = \$updated_noise_multiplier_peer1 | .maxDecisionSize = \$updated_max_decision_size_peer1 | .minDecisionSize = \$updated_min_decision_size_peer1 | .sensitivity = \$updated_sensitivity_peer1 | .dpInterval = \$updated_dp_interval_peer1 | .senderLoopInterval = \$updated_sender_loop_interval_peer1)' \
                                \$PEER1_JSON_FILE_PATH > temp.json && mv temp.json \$PEER1_JSON_FILE_PATH"
  echo -e "${GREEN}Peer1 JSON file is updated in $SSH_USER_PEER1.${OFF}"
else
  echo -e "${RED}Connection was not successful.${OFF}"
fi




# Update peer2 JSON file
echo -e "${YELLOW}Connecting to $SSH_USER_PEER2...${OFF}"
status=$(ssh -o BatchMode=yes -o ConnectTimeout=5 "$SSH_USER_PEER2" echo ok 2>&1)
if [[ $status == ok ]] ; then
  echo -e "${GREEN}Connection was successful.${OFF}"
  ssh "$SSH_USER_PEER2" "jq --argjson updated_noise_multiplier_peer2 \"$updated_noise_multiplier_peer2\" \
                                --argjson updated_sensitivity_peer2 \"$updated_sensitivity_peer2\" \
                                --argjson updated_dp_interval_peer2 \"$updated_dp_interval_peer2\" \
                                --argjson updated_sender_loop_interval_peer2 \"$updated_sender_loop_interval_peer2\" \
                                --argjson updated_max_decision_size_peer2 \"$updated_max_decision_size_peer2\" \
                                --argjson updated_min_decision_size_peer2 \"$updated_min_decision_size_peer2\" \
                                '.shapedReceiver |= (.noiseMultiplier = \$updated_noise_multiplier_peer2 | .maxDecisionSize = \$updated_max_decision_size_peer2 | .minDecisionSize = \$updated_min_decision_size_peer2 | .sensitivity = \$updated_sensitivity_peer2 | .dpInterval = \$updated_dp_interval_peer2 | .senderLoopInterval = \$updated_sender_loop_interval_peer2)' \
                                \$PEER2_JSON_FILE_PATH > temp.json && mv temp.json \$PEER2_JSON_FILE_PATH"
  echo -e "${GREEN}Peer2 JSON file is updated in $SSH_USER_PEER2.${OFF}"
else
  echo -e "${RED}Connection was not successful.${OFF}"
fi





# echo -e "${YELLOW}Experiment is started on desh01-minesvpn...${OFF}" 
# cd /home/minesvpn/workspace/minesvpn-testbed/experiments/macrobenchmarks && ./run_parallel_experiments.sh \$iter_num
