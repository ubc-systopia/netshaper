#!/bin/bash

# Colors
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
OFF='\033[0m'


# Maximum number of parallel processes for pcap to csv conversion
MAX_PARALLEL=20


traces_dir=""
csvs_dir=""
# Function to display usage information
usage() {
    echo "Usage: $0 --traces_dir <dir> --csvs_dir <dir>"
    exit 1
}
# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --traces_dir)
            traces_dir="$2"
            shift 2
            ;;
        --csvs_dir)
            csvs_dir="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done
# Check if all required arguments are provided
if [[ -z "$traces_dir" || -z "$csvs_dir" ]]; then
    echo "Missing required arguments."
    usapcap
fi



TRACES_DIR=$traces_dir
CSVS_DIR=$csvs_dir

pcaps=($(find "${TRACES_DIR}/" -iname "*.pcap" -type f -print))
for pcap in ${pcaps[@]}; do
  file=$(echo ${pcap#$TRACES_DIR})
  mkdir -p "${CSVS_DIR}/$(dirname $file)"
  csvFile=$(dirname $file)/$(basename $pcap .pcap).csv

  if echo $file | grep -q "peer1"; then
    tshark -Y "ip.dst == 192.168.2.3" \
      -Tfields \
      -e frame.time_relative -e udp.length \
      -r $pcap \
      >"${CSVS_DIR}/${csvFile}" 2>/dev/null &

  elif echo $file | grep -q "peer2"; then
    tshark -Y "ip.dst == 192.168.2.2" \
      -Tfields \
      -e frame.time_relative -e udp.length \
      -r $pcap \
      >"${CSVS_DIR}/${csvFile}" 2>/dev/null &
  fi
  #	break
  COUNTER=$(($COUNTER + 1))
  if [[ $COUNTER -ge $MAX_PARALLEL ]]; then
    echo -e "${YELLOW}Waiting for previous batch to finish${OFF}"
    wait
    COUNTER=0
  fi
done
