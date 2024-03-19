#!/bin/bash

#sed "s/\"peer2\"/\"$PEER\"/" $CONFIG > config2.json
sed "s/4567/$PEER_PORT/" $CONFIG > config2.json

#echo "Starting tcpdump for $((TIMEOUT + 15)) seconds"
#timeout -s SIGINT $((TIMEOUT + 15)) tcpdump -s $MAX_CAPTURE_SIZE -w traces/$PCAP.pcap &

echo "Starting Peer1 for $((TIMEOUT + 10)) seconds"
timeout -s SIGINT $((TIMEOUT + 10)) /root/peer_1 config2.json > traces/$PCAP.log 2>&1

# Wait for tcpdump to die
sleep 7
