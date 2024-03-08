# NetShaper: A Differentially Private Network Side-Channel Mitigation System
This repository contains the source code and instructions to reproduce the results of the paper "NetShaper: A Differentially Private Network Side-Channel Mitigation System" published in the the 33rd USENIX Security Symposium (USENIX Security '24).

📎 **Paper**: [https://arxiv.org/abs/2310.06293]
## Code
NetShaper artifact consists of two main components: A testbed that implements a setup of four machine connected with linear topology, and a simulator that models the traffic shaping mechanism to speed up measurements.

The directory structure is as follows:
- `hardware/`: Contains the code for different components of the testbed and the scripts to setup the testbed machines.
- `simulator/`: Contains the scripts to run the simulator.
- `evaluation/`: Contains the scripts to run the experiments and reproduce the results.
- `dataset/`: Contains the dataset used in the experiments.


## Usage
The evaluation directory contains the scripts to run the experiments and reproduce the results. These experiments are privacy microbenchmarks, classification of traffic traces, comparison with related work, video streaming bandwidth overhead, video streaming latency, web service bandwidth overhead, and web service latency.  

Each experiment contains a README file with the instructions to run the experiments and it contains a config file to configure the parameters for the experiment. The current configuration is set to match the parameters used in the paper.

Experimens that involve the testbed require the testbed to be setup. The testbed setup is described in the  directory of the corresponding experiment.
The testbed setup requires physical access to the machines and the ability to SSH into them. The following figure represents the testbed topology:

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
The experiments executed from the local machine and are managed through the client machine. Therefore, the client machine should have the ability to SSH into the other machines to start different stages of experiment and collect the result.





