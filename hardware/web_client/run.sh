#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'




if [[ -z $1 || -z $2 || -z $3 || -z $4 || -z $5 || -z $6 ]]
then
  echo -e "${RED}Usage ./run.sh <mode> <client_num> <request_rate> <request_size> <iter_num> <peer1-IP> ${OFF}"
  exit 1
fi


mode=$1
client_num=$2
request_rate=$3
request_size=$4
iter_num=$5
peer1_IP=$6

port=$(($iter_num + 8000))

echo -e "${YELLOW}Running the web client${OFF}"


if [[ $mode == "shaping" ]]; then
  path="latencies/iter_$iter_num"
elif [[ $mode == "no-shaping" || $mode=="baseline" ]]; then
  path="latencies/iter_$iter_num/req_$request_size"
fi

if [[ $mode == "baseline" ]]; then
  port=5555
else 
  port=$(($iter_num + 8000))
fi


mkdir -p $path

docker run \
  -d --rm \
  --name "web_client-$experiment-$client_num-$iter_num" \
  -e MODE="$mode" \
  -e CLIENT_NUM="$client_num" \
  -e REQUEST_RATE="$request_rate" \
  -e REQUEST_SIZE="$request_size" \
  -e ITER_NUM="$iter_num" \
  -e PORT="$port" \
  -e PEER1_IP="$peer1_IP" \
  -v $(pwd)/$path:/root/latencies \
  amirsabzi/netshaper:web-client
