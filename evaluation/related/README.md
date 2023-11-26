# NetShaper vs Other Shaping Mechanisms
This directory contains the code for the NetShaper vs Other Shaping Mechanisms experiment. 
We compare NetShaper with [Pacer](https://www.usenix.org/system/files/sec22-mehta.pdf) and Constant-Rate Shaping.


## Installing Python Dependencies
To install the python dependencies, run the following command:
```bash
pip install -r ../../simulator/requirements.txt
```
## Running the Experiments
To run compare bandwidth overhead of NetShaper with Pacer and Constant-Rate Shaping for the web dataset, run the following command:
```bash
./run.sh --experiment="overhead_comparison_web"  --config_file="configs/overhead_comparison_web.json"
```
To run the second microbenchmark, run the following command:
```bash
./run.sh --experiment="number_of_flows_vs_overhead_video"  --config_file="configs/number_of_flows_vs_overhead_video.json"
```
The results will be saved in the `/results` directory. The corresponding plots are saved in the `/plots` directory.