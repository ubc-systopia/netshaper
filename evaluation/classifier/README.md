# Empirical Privacy 
This directory contains the code for the empirical privacy benchmarks, where we measure the effectiveness of traffic analysis attacks with and without DP shaping.
We report the accuracy, precision and recall of the attack. The experiment requires 5 human minutes and 72 compute hours to finish.

## Installing Python Dependencies
To install the python dependencies, run the following command:
```bash
pip install -r ../../simulator/requirements.txt
```


## Configuration File
You can find the configuration file for this experiment in the `/configs` directory.
```json
{ 
  "data_source" : "processed",
  "load_data_dir_server_to_client": "../../dataset/simulator/video/cache/server_to_client/",
  "results_dir": "results/",
  "plot_dir": "plots/",
  "max_num_of_unique_streams" : 100,
  "max_num_of_traces_per_stream" : 100,

  "experiment" : "empirical_privacy",
  "communication_type": "unidirectional",
  "filter_data": true,
  "num_of_unique_streams": 40,
  "num_of_traces_per_stream": 100, 


  "transport_type": "DP_dynamic",
  "DP_mechanism": "Gaussian",
  "min_privacy_loss_server_to_client": 200,
  "max_privacy_loss_server_to_client": 200000,  
  "num_privacy_loss_server_to_client":  10,
  "delta_server_to_client": 1e-6,
  "sensitivity_server_to_client": 2.5e6,
  "dp_intervals_us": [[1e6, 1e4]],
  "privacy_window_size_server_to_client_s": 5,
  "attack_epoch_num": 1000,
  "attack_batch_size": 64,
  "attack_num_of_repeats": 3,


}
```
1. `data_source`: The type of data used for the experiment. The value can be either `raw_data` or `processed`.
2. `load_data_dir_server_to_client`: The path to the directory that contains the data files (either processed or raw data) for the server to client communication.
3. `results_dir`: The directory to save the results of the experiment.
4. `plot_dir`: The directory to save the plots of the experiment.
5. `max_num_of_unique_streams`: The maximum number of unique streams in the dataset. We need this parameter to process the data.
6. `max_num_of_traces_per_stream`: The maximum number of traces per stream in the dataset. We need this parameter to process the data.
7. `experiment`: The name of the experiment.
8. `communication_type`: The communication type between the client and the server. The value can be either `bidirectional` or `unidirectional`.
9. `filter_data`: A boolean value that indicates whether to filter the data or not. If the value is `true`, the simulator will filter the data based on two parameters: `num_of_unique_streams` and `num_of_traces_per_stream`. If the value is `false`, the simulator will not filter the data.
10. `num_of_unique_streams`: The number of unique streams to be used in the experiment. This parameter is used only if `filter_data` is set to `true`.
11. `num_of_traces_per_stream`: The number of traces per stream to be used in the experiment. This parameter is used only if `filter_data` is set to `true`.
12. `transport_type`: The shaping mechanism to be used in the experiment. The value can be either `DP_dynamic`, `DP_wPrivacy`, or `DP_static`.
13. `DP_mechanism`: The DP mechanism is used for NetShaper traffic shaping (i.e., `DP_dynamic`). The value can be either `Gaussian` or `Laplace`.
14. `min_privacy_loss_server_to_client`: The minimum value of the privacy loss for the server to client communication.
15. `max_privacy_loss_server_to_client`: The maximum value of the privacy loss for the server to client communication.
16. `num_privacy_loss_server_to_client`: The number of privacy loss values for the server to client communication.
17. `delta_server_to_client`: The delta value for the server to client communication.
18. `sensitivity_server_to_client`: The sensitivity value for the server to client communication.
19. `dp_intervals_us`: A pair of DP intervals for the server to client communication and the client to server communication in microseconds. Note that the first value is DP interval for server to client communication and the second value is the corresponding client to server DP interval.
20. `privacy_window_size_server_to_client_s`: The length of window for which simulator calculates the aggregated privacy loss for the server to client communication.
21. `attack_epoch_num`: The number of epochs for the attack.
22. `attack_batch_size`: The batch size for the attack.
23. `attack_num_of_repeats`: The number of repeats for the attack.


## Run
To run the privacy microbenchmark, run the following command:
```bash
./run.sh --experiment="empirical_privacy"  --config_file="configs/empirical_privacy.json"
```
The results will be saved in the `/results` directory.

## Experiment Results
The results of the experiments are saved in the `/results` directory. The corresponding plots are saved in the `/plots` directory.
