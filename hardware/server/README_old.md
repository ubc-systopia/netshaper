# Server 

This directory contains the scripts for a server that is used to run the experiments.

### Nginx Server
This folder contains the Nginx server. Instructions on running it are in the README within the folder.
Currently, the Nginx Server is used to serve the following:
1. Videos (for the Video experiment)
2. Text response (for latency experiment)

For a simple latency client, use the following command ` curl -X POST -d "Hello" -H 'Content-Type: text/plain' http://localhost:5555/latency`  
The command assumes that it is being called from the host of the docker container (which is listening to port 5555)  
The response from the server should be "OK" (with a newline at the end)
