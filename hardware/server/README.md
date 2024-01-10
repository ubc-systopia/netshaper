# Steps to run the server

1. Run `./setup.sh` (or ensure you have docker and docker-compose and can run them without sudo)  

### For Videos: 
2. Put your DASH encoded videos in "Dataset" folder  
  
### For both Videos and Latency:
3. Run `docker-compose up -d` (optional: Change the port from '5555' to your choice in `docker-compose.yml`)  
_Note: the latency endpoint is at /latency on the server_
