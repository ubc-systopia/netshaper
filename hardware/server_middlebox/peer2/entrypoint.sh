#!/bin/bash

#echo "Starting tcpdump for $((TIMEOUT + 15)) seconds"
#timeout -s SIGINT $((TIMEOUT + 15)) tcpdump -s $MAX_CAPTURE_SIZE -w traces/$PCAP.pcap &

echo "Starting Peer2 for $((TIMEOUT + 10)) seconds"
timeout -s SIGINT $((TIMEOUT + 10)) /root/peer_2 $CONFIG > traces/$PCAP.log 2>&1

# Wait for tcpdump to die
sleep 7
