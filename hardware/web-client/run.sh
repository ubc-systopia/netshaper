#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'


if [[ -z $1 ]]
then
  echo -e "${RED}Usage ./run.sh <iter_num>${OFF}"
  exit 1
fi




iter_num=$1
clients=128
requests=1600


echo -e "${YELLOW}Running the web client${OFF}"
mkdir -p latencies/$iter_num
./wrk2/wrk -c $clients -d 180 -R $requests -L -U -t $clients -s multiple_urls.lua http://192.168.1.2:8000/ > latencies/$iter_num/wrk.log
mv stats.csv latencies/$iter_num/stats.csv 


echo -e "${GREEN}Done!${OFF}"
