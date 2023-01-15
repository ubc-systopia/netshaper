#!/bin/bash

# Parameters to pass to each program
peer_2_unshaped="127.0.0.1
5555
10"
peer_2_shaped="10"
peer_1_unshaped="1"
peer_1_shaped="1"
echo_string="Is this working?"
# Time the script will wait for network connections to establish at each point)
# TODO: Figure out why only sleep_time = 2 or 3 work.
sleep_time=2

# /dev/null to ignore all output
# /dev/stdout to show all output
output_redirect="/dev/null"

# Colour codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
OFF='\033[0m'

# Kill all background jobs on exit
trap "kill 0" EXIT

# Make a temp folder to store output file
mkdir test
cd test || exit

# Generate temporary keys (don't care about information in the certificate)
echo -e "${YELLOW}Generating temporary certificates${OFF}"
printf "\n\n\n\n\n\n\n" |
  openssl req -nodes -new -x509 -keyout server.key -out server.cert \
  &> $output_redirect

# Run the tcp receiver (This is job 1)
echo -e "${YELLOW}Starting TCP Receiver${OFF}"
nc -l -d -p 5555 >output &

# Run the unshaped sender of peer 2 (This is job 2)
echo -e "${YELLOW}Starting Unshaped Sender (Peer 2)${OFF}"
printf '%s\n' "$peer_2_unshaped" |
  ../build/example_peer_2_unshaped &> $output_redirect &
sleep $sleep_time

# Run the shaped receiver of peer 2 (This is job 3)
echo -e "${YELLOW}Starting Shaped Receiver (Peer 2)${OFF}"
printf '%s\n' "$peer_2_shaped" |
  ../build/example_peer_2_shaped &> $output_redirect &
sleep $sleep_time

# Run the unshaped receiver of peer 1 (This is job 4)
echo -e "${YELLOW}Starting Unshaped Receiver (Peer 1)${OFF}"
printf '%s\n' "$peer_1_unshaped" |
  ../build/example_peer_1_unshaped &> $output_redirect &
sleep $sleep_time

# Run the shaped sender of peer 1 (This is job 5)
echo -e "${YELLOW}Starting Shaped Sender (Peer 1)${OFF}"
printf '%s\n' "$peer_1_shaped" |
  ../build/example_peer_1_shaped &> $output_redirect &
sleep $sleep_time

# Run the tcp sender (This is job 6)
echo -e "${YELLOW}Starting TCP Sender and sending data${OFF}"
printf '%s\n' "$echo_string" | nc -N localhost 8000
echo -e "${YELLOW}Waiting for message to propagate${OFF}"
sleep $((sleep_time + 10))

# Check if output matches input
echo -e "${YELLOW}Checking if output matches input${OFF}"
output="$(tr -d '\0' < output)" #Ignore the null bytes (dummy)
if [ "$echo_string" == "$output" ]; then
  echo -e "${GREEN}Test passed!${OFF}"
else
  echo -e "${RED}Test failed!${OFF}"
  echo -e "${RED}Expected \"$echo_string\", received \"$output\"${OFF}"
fi

# Remove the test directory
echo -e "${YELLOW}Cleaning up!${OFF}"
cd ..
rm -r test

#sleep 1000
