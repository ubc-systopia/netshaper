# Microbenchmark Experiments
(**If you already have the setup for web latency and/or video latency experiments, skip to the client section**).  
This folder contains the scripts for microbenchmarks. 
These experiments are executed on our testbed, which consists of 4 machines connected with a linear topology.
The machines are connected with 10Gbps links, two of which work as the client and the server, and the other two work as NetShaper middleboxes.

     +-----------+        +-----------+        +-----------+        +-----------+
     |   Client  |--------|    MB-1   |--------|    MB-2   |--------|   Server  |
     +-----------+ 10Gbps +-----------+ 10Gbps +-----------+ 10Gbps +-----------+


## General Prerequisites
You should have physical access to the testbed machines, as well as the ability to SSH into them. The physical access is required to change BIOS settings and reboot the machines. SSH access is required to run the experiments and collect the results.
The following configurations should be applied to all 4 machines. 

### BIOS Configuration
1. Disable hyperthreading.
2. Disable BIOS CPU Frequency control (if enabled). Some servers refer to it as OS DBPM (should be on).

### OS Configuration
Isolate at 6 CPU cores from the Linux scheduler. We isolate cores 2-8 (6 cores) in the example below (This is for Ubuntu 22.04 with a grub bootloader. If you are running a different OS, modify accordingly):
1. Open `/etc/default/grub` in a text editor.
2. Add `isolcpus=2-8` and `systemd.unified_cgroup_hierarchy=false` to the `GRUB_CMDLINE_LINUX_DEFAULT` variable.
3. Run `sudo update-grub` to update the grub configuration.
4. Reboot the system and check if `cat /proc/cmdline` has `isolcpus=2-8` at the end.

Next, disable Interrupt Request Balance (IRQBALANCE) to prevent the Linux kernel from moving the interrupts to the isolated cores:

1. Open `/etc/default/irqbalance` in a text editor.
2. Add `IRQBALANCE_BANNED_CPULIST=2-8` to the file.
3. Run `sudo systemctl restart irqbalance` to restart the irqbalance service.

Fix the CPU frequency on the specified cores, to reduce variability. In our setup, we fixed it at 4GHz (it should be fixed once after every boot).
```bash
for i in {2..8}; do cpufreq-set -c $i -f 4GHz; done
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

### Build
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

### Build
To build the server-side middlebox binary, execute the following command:
```bash
./build.sh
```
With the successful build of the middlebox, build the peer2 container with the following command:
```bash
docker build -t peer2 ./peer2/
```


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

### Build
To build the client-side middlebox binary, execute the following command:
```bash
./build.sh
```
With the successful build of the middlebox, build the peer1 container with the following command:
```bash
docker build -t peer1 ./peer1/
```


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


### Build 
To build the client container, execute the following command:
```bash
./build.sh
```

## Running the Experiments
To run the experiments, follow these steps:
1. **Ensure that the server, middleboxes, and client have been set up and built.**
2. Open an SSH connection to the client machine.
3. Change the directory to `Path/To/Netshaper/evaluation/microbenchmarks/`
4. run the experiment script with the following command:
```bash
./run.sh
```
