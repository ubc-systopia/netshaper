#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'




if [[ -z $1 || -z $2 || -z $3 || -z $4 ]]
then
  echo -e "${RED}Usage ./run.sh <experiment> <client_num> <request_rate> <request_size> <iter_num>${OFF}"
  exit 1
fi


experiment=$1
client_num=$2
request_rate=$3
request_size=$4
iter_num=$5

port=$(($iter_num + 8000))

docker run \
  -d --rm \
  --name "web_client-$experiment-$client_num-$iter_num" \
  -e EXPERIMENT="$experiment"
  -e CLIENT_NUM="$client_num"
  


