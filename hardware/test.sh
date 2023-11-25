#!/bin/bash

# Parameters to pass to each program
# Number of max queues to initialise
peer_2="10"
peer_1="1"
client_string="Client says Hello"
server_string="Server says Hello"
serverPipe=ncServerPipe # Pipe to send server_string to tcp server
clientPipe=ncClientPipe # Pipe to send client_string to tcp client

# Time the script will wait for network connections to establish at each point)
# sleep_time is the time in seconds for which it will wait
sleep_time=5

# /dev/null to ignore all serverOutput
# /dev/stdout to show all serverOutput
output_redirect="/dev/null"

# Colour codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
OFF='\033[0m'

# Kill all background jobs on exit
PIds=""
trap 'kill $PIds; exit 0' EXIT

# Make a temp folder to store serverOutput file
mkdir test
cd test || exit

# Make named pipe to send server_string to tcp server
mkfifo $serverPipe
mkfifo $clientPipe

# Generate temporary keys (don't care about information in the certificate)
echo -e "${YELLOW}Generating temporary certificates${OFF}"
printf "\n\n\n\n\n\n\n" |
  openssl req -nodes -new -x509 -keyout server.key -out server.cert \
    &>$output_redirect

# Run the tcp server
echo -e "${YELLOW}Starting TCP Receiver${OFF}"
nc -l -p 5555 <$serverPipe >serverOutput &
PIds="$PIds $!"
exec {serverPipeFd}>$serverPipe

# Run Peer 2
echo -e "${YELLOW}Starting Peer 2${OFF}"
printf '%s\n' "$peer_2" |
  ../build/src/example/peer2/peer_2 &>$output_redirect &
PIds="$PIds $!"
sleep $sleep_time

# Run Peer 1
echo -e "${YELLOW}Starting Peer 1${OFF}"
printf '%s\n' "$peer_1" |
  ../build/src/example/peer1/peer_1 &>$output_redirect &
PIds="$PIds $!"
sleep $((sleep_time + 10))

#sleep $((sleep_time + 5))

# Run the tcp client
echo -e "${YELLOW}Starting TCP Sender and sending data${OFF}"
nc localhost 8000 <$clientPipe >clientOutput &
PIds="$PIds $!"
exec {clientPipeFd}>$clientPipe
printf '%s\n' "$client_string" >&${clientPipeFd}
echo -e "${YELLOW}Waiting for message to propagate...${OFF}"
sleep $((sleep_time + 5))

# Send response from tcp server
echo -e "${YELLOW}Sending response back from server${OFF}"
printf "%s\n" "$server_string" >&${serverPipeFd}
echo -e "${YELLOW}Waiting for response to propagate...${OFF}"
sleep $((sleep_time + 5))
eval "exec $clientPipeFd>&-"
eval "exec $serverPipeFd>&-"

# Check if serverOutput matches input
echo -e "${YELLOW}Checking if outputs match inputs${OFF}"
serverOutput="$(tr -d '\0' <serverOutput)" #Ignore the null bytes (dummy)
clientOutput="$(tr -d '\0' <clientOutput)" #Ignore the null bytes (dummy)
if [ "$client_string" == "$serverOutput" ] &&
  [ "$server_string" == "$clientOutput" ]; then
  echo -e "${GREEN}Test passed!${OFF}"
else
  echo -e "${RED}Test failed!${OFF}"
  echo -e "${RED}Expected server to receive \"$client_string\", received \"$serverOutput\"${OFF}"
  echo -e "${RED}Expected client to receive \"$server_string\", received \"$clientOutput\"${OFF}"
fi

# Remove the test directory
echo -e "${YELLOW}Cleaning up!${OFF}"
cd ..
rm -r test
