# NetShaper: A Differentially Private Network Side-Channel Mitigation System
This repository contains the source code and instructions to reproduce the results of the paper "NetShaper: A Differentially Private Network Side-Channel Mitigation System" published in the the 33rd USENIX Security Symposium (USENIX Security '24).

📎 **Paper**: [https://www.usenix.org/conference/usenixsecurity24/presentation/sabzi]
## Code
NetShaper artifact consists of two main components: A testbed that implements a setup of four machine connected with linear topology, and a simulator that models the traffic shaping mechanism to speed up measurements.

The directory structure is as follows:
- `hardware/`: Contains the code for different components of the testbed and the scripts to setup the testbed machines.
- `simulator/`: Contains the scripts to run the simulator.
- `evaluation/`: Contains the scripts to run the experiments and reproduce the results.
- `dataset/`: Contains the dataset used in the experiments.


## Dataset
The dataset used in the experiments is available at [https://zenodo.org/records/10783814](https://zenodo.org/records/10783814), and it should be downloaded and extracted to the `dataset/` directory on client and server machine and the machine that executes the simulator. 

## Usage
### Simulator
The evaluation directory contains the scripts to run the experiments on the simulator and reproduce the results.
These experiments are privacy microbenchmarks, classification of traffic traces, comparison with related work, video streaming bandwidth overhead, and web service bandwidth overhead.

The simulator requires a machine that
should have at least 8 CPU cores, 64 GB of RAM, and a
GPU with 24 GB of memory. Simulator also requires Python 3.10.6 and can be executed on arbitrary OS without root access. The list of required Python packages is available in [requirments.txt](simulator/requirements.txt). 

Each experiment contains a README file with the instructions to run the experiments and it contains a config file to configure the parameters for the experiment. The current configuration is set to match the parameters used in the paper.

### Testbed
The evaluation directory contains the scripts and instructions to set up the testbed and run the experiments on the testbed to reproduce the results. Video streaming latency, web service latency, and microbenchmarks are executed on the testbed.





These experiments require the testbed to be setup. The testbed setup is described in the  directory of the corresponding experiment.
The testbed setup requires physical access to the machines and the ability to SSH into them. Each machine should have at least 8 CPU
cores, 32 GB of RAM, and a 10 Gbps NIC. The following figure represents the testbed topology:

```
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
```
Our implementation is specific
to Ubuntu 22.04. For video server, we use Nginx 1.23.4.
For transport protocol we use Microsoft implementation
of the QUIC protocol, MsQUIC 2.2.4. 

Each experiment contains a README file with the instructions to set up the server, configure it, and run the experiments. It also contains a config file to configure the parameters for the experiment. The current configuration is set to match the parameters used in the paper

The experiments executed from the local machine and are managed through the client machine. Therefore, **the client machine should have the ability to SSH into the other machines** to start different stages of experiment and collect the result.





