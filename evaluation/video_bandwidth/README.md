# Bandwidth overhead and latency for video application 
This directory contains the code for evaluating the bandwidth overhead of NetShaper with video streaming application.  


## Installing Python Dependencies
To install the python dependencies, run the following command:
```bash
pip install -r ../../simulator/requirements.txt
```
## Running the Experiments
To calculate the bandwidth overhead of NetShaper for different values of `dp_interval`, run the following command:
```bash
./run.sh --experiment="dp_interval_vs_overhead_video"  --config_file="configs/dp_interval_vs_overhead_video.json"
```
The results will be saved in the `/results` directory. The corresponding plots are saved in the `/plots` directory.