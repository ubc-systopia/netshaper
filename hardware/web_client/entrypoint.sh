#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
HGREEN='\033[7;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
OFF='\033[0m'




# if [[ -z $1 || -z $2 || -z $3 || -z $4 ]]
# then
#   echo -e "${RED}Usage ./run.sh <experiment> <client_num> <request_rate> <request_size> <iter_num>${OFF}"
#   exit 1
# fi



# client_num=$2
# request_rate=$3
# request_size=$4
# iter_num=$5

# port=$(($iter_num + 8000))


if [[ $EXPERIMENT == "web-latency" ]]; then
  echo -e "${YELLOW}Measuring the latency for a web service.${OFF}"


  echo -e "${YELLOW}Running the web client${OFF}"
  mkdir -p "latencies/iter_$ITER_NUM"

  ./wrk2/wrk -c $CLIENT_NUM -d 180 -R $REQUEST_RATE -L -U -t $CLIENT_NUM -s multiple_urls.lua "http://192.168.1.2:$PORT/" > "latencies/iter_$ITER_NUM/wrk.log"

  mv stats.csv "latencies/iter_$ITER_NUM/stats.csv" 

  echo -e "${GREEN}Done!${OFF}"

elif [[ $EXPERIMENT == "microbenchmark" ]]; then
  echo -e "${YELLOW}Measuring the latency for a microbenchmark.${OFF}"

  echo -e "${YELLOW}Running the web client${OFF}"
  mkdir -p "latencies/iter_$ITER_NUM/req_$REQUEST_SIZE"

  ./wrk2/wrk -c $CLIENT_NUM -d 120 -R $REQUEST_RATE -L -U -t $CLIENT_NUM "http://192.168.1.2:$PORT/web/latency/$REQUEST_SIZE.html" > "latencies/iter_$ITER_NUM/req_$REQUEST_SIZE/wrk.log"

  mv stats.csv "latencies/iter_$ITER_NUM/req_$REQUEST_SIZE/stats.csv"

fi





