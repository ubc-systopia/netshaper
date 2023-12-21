# Privacy Microbenchmarks
This directory contains the code for the emprical privacy benchmarks, where we measure the effectiveness of traffic analysis attacks with and without DP shaping.
We report the accuracy, precision and recall of the attack.

## Installing Python Dependencies
To install the python dependencies, run the following command:
```bash
pip install -r ../../simulator/requirements.txt
```

## Running the Microbenchmarks
To run the first microbenchmark, run the following command:
```bash
./run.sh --experiment="empirical_privacy"  --config_file="configs/empirical_privacy.json"
```
The results will be saved in the `/results` directory.

## Configuration File
You can find the configuration file for each microbenchmark in the `/configs` directory.

## Experiment Results
The results of the experiments are saved in the `/results` directory. The corresponding plots are saved in the `/plots` directory.
