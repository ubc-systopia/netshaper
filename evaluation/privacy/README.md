# Privacy Microbenchmarks
This directory contains the code for the privacy microbenchmarks.
Here, we measure the relation between standard deviation of the noise added to the queue size and the privacy loss, and the relation between number of DP queries (i.e. number of times the algorithm measures the size of the queue) and the privacy loss.


## Prerequisites
To install the python dependencies, run the following command:
```bash
pip install -r ../../simulator/requirements.txt
```

## Configuration File
You can find the configuration file for each microbenchmark in the `/configs` directory.
### Privacy loss VS noise standard deviation
```json
{
  "sensitivity" : 100e3,
  "delta" : 1e-6,
  "num_of_queries" : [1, 4, 16, 64],
  "results_dir": "results/",
  "plot_dir": "plots/"
}
```

### Privacy loss VS number of queries
```json
{
  "sensitivity" : 100e3,
  "delta" : 1e-6,
  "noise_variances" : [100e3, 400e3, 1600e3, 6400e3],
  "results_dir": "results/",
  "plot_dir": "plots/"
}
```
*. `sensitivity`: The sensitivity value for differentially private traffic shaping.
*. `delta`: The delta value for differentially private traffic shaping.
*. `num_of_queries`: A list of number of queries (i.e. number of times the algorithm measures the size of the queue) for the experiment.
*. `results_dir`: The directory to save the results of the experiment.
*. `plot_dir`: The directory to save the plots of the experiment.
*. `noise_variances`: A list of standard deviations of the noise added to the queue size for the experiment.

## Run
To run the first microbenchmark, run the following command:
```bash
./run.sh --experiment="privacy_loss_vs_noise_std"  --config_file="configs/privacy_loss_vs_noise_std.json"
```

To run the second microbenchmark, run the following command:
```bash
./run.sh --experiment="privacy_loss_vs_query_num"  --config_file="configs/privacy_loss_vs_query_num.json"
```
The results will be saved in the `/results` directory.

## Experiment Results
The results of the experiments are saved in the `/results` directory. The corresponding plots are saved in the `/plots` directory.
