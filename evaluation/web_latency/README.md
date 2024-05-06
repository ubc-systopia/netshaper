# Web Streaming Latency Experiments
(**If you already have the setup for video latency and/or microbenchmarks experiments, skip to the client section**).  
This folder contains the scripts for the web streaming latency experiments. 
These experiments are executed on our testbed, which consists of 4 machines connected with a linear topology.
The machines are connected with 10Gbps links, two of which work as the client and the server, and the other two work as NetShaper middleboxes.


         +-----------+        +-----------+        +-----------+        +-----------+
         |   Client  |--------|    MB-1   |--------|    MB-2   |--------|   Server  |
         +-----------+ 10Gbps +-----------+ 10Gbps +-----------+ 10Gbps +-----------+
               |                    |                    |                    |
               |                    |                    |                    |
                                        SSH Connections  
               |                    |                    |                    |
               |                    |                    |                    |
               v                    v                    v                    v
         +--------------------------------------------------------------------------+
         |                           User (local machine)                           |
         +--------------------------------------------------------------------------+



## General Prerequisites
You should have physical access to the testbed machines, as well as the ability to SSH into them. The physical access to middlebox machines is required to change BIOS settings and reboot the machines. SSH access is required to run the experiments and collect the results.
The following configurations should be applied to machines hosting middleboxes (MB-1 and MB-2). 

This experiment requires 30 human minutes and 2 compute hours to finish.

### BIOS Configuration
1. Disable hyperthreading.
2. Disable BIOS CPU Frequency control (if enabled). Some servers refer to it as OS DBPM (should be on).

### OS Configuration
Isolate at 6 CPU cores from the Linux scheduler. We isolate cores 2-7 (6 cores) in the example below (This is for Ubuntu 22.04 with a grub bootloader. If you are running a different OS, modify accordingly):
1. Open `/etc/default/grub` in a text editor.
2. Add `isolcpus=2-7` and `systemd.unified_cgroup_hierarchy=false` to the `GRUB_CMDLINE_LINUX_DEFAULT` variable.
3. Run `sudo update-grub` to update the grub configuration.
4. Reboot the system and check if `cat /proc/cmdline` has `isolcpus=2-7` at the end.

Next, disable Interrupt Request Balance (IRQBALANCE) to prevent the Linux kernel from moving the interrupts to the isolated cores:

1. Open `/etc/default/irqbalance` in a text editor.
2. Add `IRQBALANCE_BANNED_CPULIST=2-7` to the file.
3. Run `sudo systemctl restart irqbalance` to restart the irqbalance service.

Fix the CPU frequency on the specified cores, to reduce variability. In our setup, we fixed it at 4GHz (it should be fixed once after every boot).
```bash
for i in {2..7}; do cpufreq-set -c $i -f 4GHz; done
```

## Server
### Prerequisites 
Ensure that Docker and Docker Compose are installed on the server machine.
```bash
sudo apt install docker-compose
```

### Setup
To set up the server, follow these steps:  
1. Open an SSH connection to the machine that serves as the server.
2. Clone the NetShaper repository onto the server machine. 
3. Change the directory to `Path/To/Netshaper/hardware/server/`
4. Execute the server setup script. Ensure that your machine has an internet connection to download the dataset for the server:
```bash
./setup.sh
```

### Run
Run the docker container of the server with the following command:
```bash
docker-compose up -d
```


## Middlebox at the server side (MB-2)
### Prerequisites
Ensure that Docker, Docker Compose, and tcpdump are installed on the middlebox machine.
```bash
sudo apt install docker-compose tcpdump
```

### Setup
To set the middlebox on the server side follow these steps:
1. Open an SSH connection to the machine that serves as the middlebox on the server side. 
2. Clone the NetShaper repository onto the middlebox on the server side.
3. Change the directory to `Path/To/NetShaper/hardware/server_middlebox/`
4. Execute the setup script. Ensure that the machine has an Internet connection to download and install dependencies. 
```bash
./setup.sh
```
The setup script will pull the server-side middlebox containers from the Docker Hub. Alternatively, you can build the server-side middlebox container based on the following instructions.

### Build (Optional)
To build the server-side middlebox binary, execute the following command:
```bash
./build.sh
```
With the successful build of the middlebox, build the peer2 container with the following command:
```bash
docker build -t amirsabzi/netshaper:peer2-shaping ./peer2/
```
### Configuration
All configuration parameters of the middlebox are provided in a JSON file name [peer2_config.json](../../hardware/server_middlebox/peer2_config.json). The description of all these configurations are provided in [configuration.md](../../hardware/configuation.md). Note that this is not necessarily the same configuration used for this particular experiment. The Python script [set_params.py](helpers/set_params.py) is used to set the parameters for middleboxes according to the configuration file of the experiment, [video_latency.json](configs/video_latency.json).

## Middlebox at the client side (MB-1)
### Prerequisites
Ensure that Docker, Docker Compose, and tcpdump are installed on the middlebox machine.
```bash
sudo apt install docker-compose tcpdump
```

### Setup
To set up the middlebox on the client side follow these steps:
1. Open an SSH connection to the machine that serves as the middlebox on the client side. 
2. Clone the NetShaper repository onto the middlebox on the client side.
3. Change the directory to `Path/To/NetShaper/hardware/client_middlebox/`
4. Execute the setup script. Ensure that the machine has an Internet connection to download and install dependencies. 
```bash
./setup.sh
```
The setup script will pull the client-side middlebox containers from the Docker Hub. Alternatively, you can build the client-side middlebox container based on the following instructions.

### Build (Optional)
To build the client-side middlebox binary, execute the following command:
```bash
./build.sh
```
With the successful build of the middlebox, build the peer1 container with the following command:
```bash
docker build -t amirsabzi/netshaper:peer1-shaping ./peer1/
```
### Configuration
All configuration parameters of the middlebox are provided in a JSON file name [peer1_config.json](../../hardware/client_middlebox/peer1_config.json). The description of all these configurations is provided in [configuration.md](../../hardware/configuation.md). Note that this is not necessarily the same configuration used for this particular experiment. The Python script [set_params.py](helpers/set_params.py) is used to set the parameters for middleboxes according to the configuration file of the experiment, [video_latency.json](configs/video_latency.json).

**Note**: Peer1 receives the IP address of the server and the port number of the server from the configuration file, currently set as `localhost:5555`. If you are running the experiment on a different set of machines, you should change the IP address and port number in the configuration file accordingly.

## Client
### Prerequisites
Ensure that Docker, Docker Compose, and Python are installed on the client machine.
```bash
sudo apt install docker-compose python3 python3-pip
```

### Setup
To setup the client, follow these steps:  
1. Open an SSH connection to the machine that serves as the client.
2. Clone the NetShaper repository onto the client machine. 
3. Change the directory to `Path/To/Netshaper/hardware/web_client/`
4. Execute the client setup script. Ensure that your machine has an internet connection to download the dataset for the client:
```bash
./setup.sh
```
The setup script will pull the client container from the Docker Hub. Alternatively, you can build the client container based on the following instructions.


### Build (Optional) 
To build the client container, first execute the following command to build the wrk2 client binary:
```bash
./build.sh
```
Then, build the client container with the following command:
```bash
docker build -t amirsabzi/netshaper:web-client .
```

### Configuration
The run script for the web client, [hardware/web_client/run.sh](../../hardware/web_client/run.sh), receives the configuration parameters as arguments. This includes the shaping mode, number of parallel clients, request rate, request size, number of iterations, and IP address of the middlebox on the client side. 

## Running the Experiments

### Experiment Configuration
The following parameters are used to configure the experiments:
```json
{
  "iter_num": 3,
  "max_client_numbers": [1, 16, 128],
  "request_rate": 1600,
  "privacy_loss_peer1": 1,
  "privacy_loss_peer2": 1,
  "sensitivity_peer1": 200,
  "sensitivity_peer2": 60000,
  "dp_interval_peer1": 10000,
  "sender_loop_interval_peer1": 0,
  "dp_intervals_peer2": [10000, 50000, 100000],
  "sender_loop_interval_peer2": 0,
  "max_decision_size_peer1": 10000000,
  "min_decision_size_peer1": 0,
  "max_decision_size_peer2": 100000000,
  "min_decision_size_peer2": 0 
}
```
1. `iter_num`: The number of iterations for each configuration.
2. `max_client_numbers`: The number of parallel clients.
3. `request_rate`: The request rate of the clients.
4. `privacy_loss_peer1`: The privacy loss parameter for the middlebox on the client side (client-to-server communication).
5. `privacy_loss_peer2`: The privacy loss parameter for the middlebox on the server side (server-to-client communication).
6. `sensitivity_peer1`: The sensitivity parameter for the middlebox on the client side (client-to-server communication).
7. `sensitivity_peer2`: The sensitivity parameter for the middlebox on the server side (server-to-client communication).
8. `dp_interval_peer1`: The differential privacy interval for the middlebox on the client side (client-to-server communication).
9. `sender_loop_interval_peer1`: The sender loop interval for the middlebox on the client side (client-to-server communication).
10. `dp_intervals_peer2`: A list of different differential privacy intervals for the middlebox on the server side (server-to-client communication).
11. `sender_loop_interval_peer2`: The sender loop interval for the middlebox on the server side (server-to-client communication).
12. `max_decision_size_peer1`: The maximum decision size for the middlebox on the client side (client-to-server communication).
13. `min_decision_size_peer1`: The minimum decision size for the middlebox on the client side (client-to-server communication).
14. `max_decision_size_peer2`: The maximum decision size for the middlebox on the server side (server-to-client communication).
15. `min_decision_size_peer2`: The minimum decision size for the middlebox on the server side (server-to-client communication).



### Run Script Parameters
To enhance the readability and maintainability of the run script, we do not use command line arguments for the run script. Instead, the first section of the run script is used to set the parameters for the experiment. The parameters are set as follows:
```bash
# ************************************************************
# *                    Run Parameters                        *
# ************************************************************
peer2_ssh_host="desh03"
peer2_ssh_username="minesvpn"

# SSH Host and Username for client-side middlebox (peer1)
peer1_ssh_host="desh02"
peer1_ssh_username="minesvpn"

peer1_IP="192.168.1.2"

# NetShaper directory at server-side middlebox (peer2)
peer2_netshaper_dir="/home/minesvpn/workspace/artifact_evaluation/code/minesvpn"

# NetShaper directory at client-side middlebox (peer1)
peer1_netshaper_dir="/home/minesvpn/workspace/artifact_evaluation/code/minesvpn"
#************************************************************
```
1. `peer2_ssh_host`: The hostname of the middlebox on the server side. For example, `desh03`.
2. `peer2_ssh_username`: The username for the middlebox on the server side. For example, `minesvpn`.
3. `peer1_ssh_host`: The hostname of the middlebox on the client side. For example, `desh02`.
4. `peer1_ssh_username`: The username for the middlebox on the client side. For example, `minesvpn`.
5. `peer1_IP`: The IP address of the middlebox on the client side. For example, `192.168.1.2`.
6. `peer2_netshaper_dir`: The absolute directory path of the NetShaper repository at the middlebox on the server side. For example, `/home/minesvpn/workspace/artifact_evaluation/code/minesvpn`.
7. `peer1_netshaper_dir`: The absolute directory path of the NetShaper repository at the middlebox on the client side. For example, `/home/minesvpn/workspace/artifact_evaluation/code/minesvpn`.

**NOTE: if you are using a different setup, you should modify the run script accordingly.**



### Run
To run the experiments, follow these steps:
1. **Ensure that the server, middleboxes, and client have been set up and built.**
2. Open an SSH connection to the client machine.
3. Change the directory to `Path/To/Netshaper/evaluation/web-latency/`
4. run the experiment script with the following command:
```bash
./run.sh
```
