# Bandwidth overhead and latency for video application 
This directory contains the code for evaluating the bandwidth overhead and latency of NetShaper with web s ervice application.  


## Installing Python Dependencies
To install the python dependencies, run the following command:
```bash
pip install -r ../../simulator/requirements.txt
```
## Running the Experiments
To calculate the bandwidth overhead of NetShaper for different values of `dp_interval`, run the following command:
```bash
./run.sh --experiment="dp_interval_vs_overhead_web"  --config_file="configs/dp_interval_vs_overhead_web.json"
```
To calculate the end-to-end latency of NetShaper for different values of `dp_interval`, run the following command:
```bash
./run.sh --experiment="dp_interval_vs_latency_web"  --config_file="configs/dp_interval_vs_latency_web.json"
```
The results will be saved in the `/results` directory. The corresponding plots are saved in the `/plots` directory.