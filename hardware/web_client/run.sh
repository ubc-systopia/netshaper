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



client_num=$2
request_rate=$3
request_size=$4
iter_num=$5

port=$(($iter_num + 8000))


if [[ $1 == "web-latency" ]]; then
  echo -e "${YELLOW}Measuring the latency for a web service.${OFF}"


  echo -e "${YELLOW}Running the web client${OFF}"
  mkdir -p "latencies/iter_$iter_num"

  ./wrk2/wrk -c $client_num -d 180 -R $request_rate -L -U -t $client_num -s multiple_urls.lua "http://192.168.1.2:$port/" > "latencies/iter_$iter_num/wrk.log"

  mv stats.csv "latencies/iter_$iter_num/stats.csv" 

  echo -e "${GREEN}Done!${OFF}"

elif [[ $1 == "microbenchmark" ]]; then
  echo -e "${YELLOW}Measuring the latency for a microbenchmark.${OFF}"

  echo -e "${YELLOW}Running the web client${OFF}"
  mkdir -p "latencies/iter_$iter_num/req_$request_size"

  ./wrk2/wrk -c $client_num -d 120 -R $request_rate -L -U -t $client_num "http://192.168.1.2:8000/web/latency/$request_size.html" > "latencies/iter_$iter_num/req_$request_size/wrk.log"

  mv stats.csv "latencies/iter_$iter_num/req_$request_size/stats.csv"

fi





