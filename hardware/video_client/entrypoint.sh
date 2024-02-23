#!/bin/bash

echo "Starting tcpdump for time $((TIMEOUT + 2))"
timeout -s SIGINT $((TIMEOUT + 2)) tcpdump -s $MAX_CAPTURE_SIZE -w traces/$PCAP.pcap &

# Wait for tcpdump to begin
sleep 1

echo "Starting playback of $VIDEO for $TIMEOUT seconds from $SERVER"
#timeout -s SIGINT $TIMEOUT ./videoClient --url http://$SERVER/$VIDEO
timeout -s SIGINT $TIMEOUT python -u video_client.py --url http://$SERVER/$VIDEO > /root/traces/$PCAP.log

# Wait for tcpdump to die
sleep 2