# Microbenchmark Experiments
This folder contains the scripts for microbenchmarks. 
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
You should have physical access to the testbed machines, as well as the ability to SSH into them. The phyical access to middlebox machines is requried to change BIOS settings and reboot the machines. The SSH access is required to run the experiments and collect the results.
The following configurations should be applied to machines hosting middleboxes (MB-1 and MB-2). 

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
The setup script will pull the server container from the Docker Hub.

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
To disable shaping and measure latency and throughput, in directory `Path/To/NetShaper/hardware/server_middlebox/`, make sure that you commented out the following line from the `CMakeLists.txt` file:
```cmake
# add_compile_definitions(SHAPING) 
```

To build the server-side middlebox binary, execute the following command:
```bash
./build.sh
```
With the successful build of the middlebox, build the peer2 container with the following command:
```bash
docker build -t amirsabzi/netshaper:peer2-no-shaping ./peer2/
```
### Configuration
All configuration parameters of the middlebox are provided in a json file name [peer2_config.json](../../hardware/server_middlebox/peer2_config.json). The description of all these configurations are provided in [configuration.md](../../hardware/configuation.md). Note that this is not necessarily the same configuration used for this particular experiment. The python script [set_params.py](helpers/set_params.py) is used to set the parameters for middlebxes according to the configuration files of the experiment, [microbenchmarks_large_requests.json](configs/microbenchmarks_large_requests.json) and [microbenchmarks_small_requests.json](configs/microbenchmarks_small_requests.json).



## Middlebox at the client side (MB-1)
### Prerequisites
Ensure that Docker, Docker Compose, and tcpdump are installed on the middlebox machine.
```bash
sudo apt install docker-compose tcpdump
```

### Setup
To set up the middlebox at the client side follow these steps:
1. Open an SSH connection to the machine that serves as the middlebox on the client side. 
2. Clone the NetShaper repository onto the middlebox on the client side.
3. Change the directory to `Path/To/NetShaper/hardware/client_middlebox/`
4. Execute the setup script. Ensure that the machine has an Internet connection to download and install dependencies. 
```bash
./setup.sh
```
The setup script will pull the client-side middlebox containers from the Docker Hub. Alternatively, you can build the client-side middlebox container based on the following instructions.
### Build (Optional)
To disable shaping and measure latency and throughput, in directory `Path/To/NetShaper/hardware/client_middlebox/`, make sure that you commented out the following line from the `CMakeLists.txt` file:
```cmake
# add_compile_definitions(SHAPING) 
```


To build the client-side middlebox binary, execute the following command:
```bash
./build.sh
```
With the successful build of the middlebox, build the peer1 container with the following command:
```bash
docker build -t amirsabzi/netshaper:peer1-no-shaping ./peer1/
```

### Configuration
All configuration parameters of the middlebox are provided in a json file name [peer1_config.json](../../hardware/client_middlebox/peer1_config.json). The description of all these configurations are provided in [configuration.md](../../hardware/configuation.md). Note that this is not necessarily the same configuration used for this particular experiment.
The python script [set_params.py](helpers/set_params.py) is used to set the parameters for middlebxes according to the configuration files of the experiment, [microbenchmarks_large_requests.json](configs/microbenchmarks_large_requests.json) and [microbenchmarks_small_requests.json](configs/microbenchmarks_small_requests.json).

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
To build the client container, execute the following command:
```bash
docker build -t amirsabzi/netshaper:web-client .
```
### Configuration
The run script for web cliet, [hardware/web_client/run.sh](../../hardware/web_client/run.sh), receives the configuration parameters as arguments. This includes the shaping mode, number of parallel clients, request rate, request size, number of iterations, and IP address of the middlebox at client side. 


## Running the Experiments

### Experiment Configuration
The following parameters are used to configure the experiments:
```json
{
  "iter_num": 3,
  "max_client_numbers": [1, 4, 16, 64, 256, 1024],
  "request_rate": 30000,
  "request_size": 1460
}
```
1. `iter_num`: The number of iterations for each experiment.
2. `max_client_numbers`: The number of parallel clients used in the experiment.
3. `request_rate`: The request rate of each client in the experiment.
4. `request_size`: The size of each request in the experiment.
The rest of parameters are set to be zero or empty, as they do not affect the results of this experiment.

### Run Script Parameters
To enhance the readability and maintainability of the run script, we do not use command line arguments for the run script. Instead, the first section of the run script is used to set the parameters for the experiment. The parameters are set as follows:

```bash

# ************************************************************
# *                    Run Parameters                        *
# ************************************************************

# SSH Host and Username for server-side middlebox (peer2)
peer2_ssh_host="desh03"
peer2_ssh_username="minesvpn"

# SSH Host and Username for client-side middlebox (peer1)
peer1_ssh_host="desh02"
peer1_ssh_username="minesvpn"

peer1_IP="192.168.1.2"
server_IP="192.168.3.4"

# NetShaper directory at server-side middlebox (peer2)
peer2_netshaper_dir="/home/minesvpn/workspace/artifact_evaluation/code/minesvpn"

# NetShaper directory at client-side middlebox (peer1)
peer1_netshaper_dir="/home/minesvpn/workspace/artifact_evaluation/code/minesvpn"

# Config directory
config_file_dir="$(pwd)/configs/microbenchmarks_small_requests.json"

#************************************************************
```
The parameters are set as follows:
1. `peer2_ssh_host`: The hostname of the server-side middlebox.
2. `peer2_ssh_username`: The username for the server-side middlebox.
3. `peer1_ssh_host`: The hostname of the client-side middlebox.
4. `peer1_ssh_username`: The username for the client-side middlebox.
5. `peer1_IP`: The IP address of the client-side middlebox.
6. `server_IP`: The IP address of the server (For measuring the latency of direct communication as baseline).
7. `peer2_netshaper_dir`: The directory of the NetShaper repository at the server-side middlebox.
8. `peer1_netshaper_dir`: The directory of the NetShaper repository at the client-side middlebox.
9. `config_file_dir`: The directory of the configuration file for the experiment (e.g., `microbenchmarks_small_requests.json` or `microbenchmarks_large_requests.json`).

**Note**: The IP addresses of the middleboxes and the server should be set according to the actual IP addresses of the machines in the testbed.

### Run
To run the experiments, follow these steps:
1. **Ensure that the server, middleboxes, and client have been set up and built.**
2. Open an SSH connection to the client machine.
3. Change the directory to `Path/To/Netshaper/evaluation/microbenchmarks/`
4. run the experiment script with the following command:
```bash
./run.sh
```
