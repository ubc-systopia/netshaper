# Privacy Microbenchmarks
This directory contains the code for the privacy microbenchmarks.
Here, we measure the relation between standard deviation of the noise asdded to the queue size and the privacy loss, and the relation between number of DP queries (i.e. number of times the algorithm measures the size of the queue) and the privacy loss.


## Installing Python Dependencies
To install the python dependencies, run the following command:
```bash
pip install -r ../../simulator/requirements.txt
```

## Running the Microbenchmarks
To run the first microbenchmark, run the following command:
```bash
./run.sh --experiment="privacy_loss_vs_noise_std"  --config_file="configs/privacy_loss_vs_noise_std.json"
```
To run the second microbenchmark, run the following command:
```bash
./run.sh --experiment="privacy_loss_vs_num_queries"  --config_file="configs/privacy_loss_vs_num_queries.json"
```
The results will be saved in the `/results` directory.

## Configuration File
You can find the configuration file for each microbenchmark in the `/configs` directory.

## Experiment Results
The results of the experiments are saved in the `/results` directory. The corresponding plots are saved in the `/plots` directory.
