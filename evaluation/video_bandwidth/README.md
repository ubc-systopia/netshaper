# Bandwidth overhead video application 
This directory contains the code for evaluating the bandwidth overhead of NetShaper with video streaming application.  


## Prerequisites
To install the python dependencies, run the following command:
```bash
pip install -r ../../simulator/requirements.txt
```

# Configuration 
You can find the configuration file for this experiment in the `/configs` directory. These are the parameters in the paper: 
```json
{ 
  "data_source": "processed",
  "load_data_dir_server_to_client": "../../dataset/simulator/video/cache/server_to_client/",
  "load_data_dir_client_to_server": "../../dataset/simulator/video/cache/client_to_server/",
  "results_dir": "results/",
  "pacer_data_dir": "../../dataset/simulator/video/pacer/tvseries_720p_sizes.csv", 
  "plot_dir": "plots/",
  "plot_logscale": false,
  "max_num_of_unique_streams": 100,
  "max_num_of_traces_per_stream": 100,

  "experiment": "dp_interval_vs_overhead_video",
  "communication_type": "bidirectional",
  "num_of_unique_streams": 100,
  "num_of_traces_per_stream": 100,
  "filter_data": "true",

  "transport_type": "DP_dynamic",
  "DP_mechanism" : "Gaussian",
  "max_video_num": 1000,
  "video_nums_list": [1, 16, 128],
  "max_videos_per_single_experiment": 10,
  "privacy_losses_server_to_client": [1], 
  "privacy_losses_client_to_server": [1],
  "delta_server_to_client": 1e-6,
  "delta_client_to_server": 1e-6,
  "sensitivity_server_to_client": 2.5e6,
  "sensitivity_client_to_server": 200,
  "min_dp_decision_server_to_client": 0,
  "min_dp_decision_client_to_server": 0,
  "static_max_dp_decision": false,
  "dp_intervals_us": [[1e6, 1e4], [5e5, 1e4], [1e5, 1e4]] ,
  "privacy_window_size_server_to_client_s": 5,
  "privacy_window_size_client_to_server_s": 1,
}
```
1. `data_source`: The type of data used for the experiment. The value can be either `raw_data` or `processed`.
2. `load_data_dir_server_to_client`: The path to the directory that contains the data files (either processed or raw data) for the server to client communication.
3. `load_data_dir_client_to_server`: The path to the directory that contains the data files (either processed or raw data) for the client to server communication.
4. `pacer_data_dir`: The path to the directory that contains the data file for Pacer.
5. `results_dir`: The directory to save the results of the experiment.
6. `plot_dir`: The directory to save the plots of the experiment.
7. `max_num_of_unique_streams`: The maximum number of unique streams in the dataset. We need this parameter to process the data.
8. `max_num_of_traces_per_stream`: The maximum number of traces per stream in the dataset. We need this parameter to process the data.
9. `experiment`: The name of the experiment.
10. `communication_type`: The communication type between the client and the server. The value can be either `bidirectional` or `unidirectional`.
11. `filter_data`: A boolean value that indicates whether to filter the data or not. If the value is `true`, the simulator will filter the data based on two parameters: `num_of_unique_streams` and `num_of_traces_per_stream`. If the value is `false`, the simulator will not filter the data.
12. `num_of_unique_streams`: The number of unique streams to be used in the experiment. This parameter is used only if `filter_data` is set to `true`.
13. `num_of_traces_per_stream`: The number of traces per stream to be used in the experiment. This parameter is used only if `filter_data` is set to `true`.
14. `transport_type`: The shaping mechanism to be used in the experiment. The value can be either `DP_dynamic`, `DP_wPrivacy`, or `DP_static`.
15. `DP_mechanism`: The DP mechanism is used for NetShaper traffic shaping (i.e., `DP_dynamic`). The value can be either `Gaussian` or `Laplace`.
16. `max_video_num`: The maximum number of videos to be used in the experiment.
17. `video_nums_list`: A list of number of videos to be used in the experiment.
18. `max_videos_per_single_experiment`: The maximum number of videos per single experiment.
19. `privacy_losses_server_to_client`: A list of privacy losses (i.e., epsilon values) for the server to client communication in multiple runs of the same experiment.
20. `privacy_losses_client_to_server`: A list of privacy losses (i.e., epsilon values) for the client to server communication in multiple runs of the same experiment.
21. `delta_server_to_client`: The delta value for the server to client communication.
22. `delta_client_to_server`: The delta value for the client to server communication.
23. `sensitivity_server_to_client`: The sensitivity value for the server to client communication.
24. `sensitivity_client_to_server`: The sensitivity value for the client to server communication.
25. `min_dp_decision_server_to_client`: The minimum value of the DP decision for the server to client communication.
26. `min_dp_decision_client_to_server`: The minimum value of the DP decision for the client to server communication.
27. `static_max_dp_decision`: This variable determines if the maximum value of the DP decision is statistically defined or not. If the value is `true`, the maximum DP decision is the maximum rate of flows in the dataset multiplied by the maximum number of parallel flows (i.e. 1000 in our setup). If the value is `false`, the maximum DP decision is the maximum rate of flows in the dataset multiplied by the current number of parallel flows.
28. `dp_intervals_us`: A pair of DP intervals for the server to client communication and the client to server communication in microseconds. Note that the first value is DP interval for server to client communication and the second value is the corresponding client to server DP interval.
29. `privacy_window_size_server_to_client_s`: The length of window for which simulator calculates the aggregated privacy loss for the server to client communication.
30. `privacy_window_size_client_to_server_s`: The length of window for which simulator calculates the aggregated privacy loss for the client to server communication.



## Run
To calculate the bandwidth overhead of NetShaper for different values of `dp_interval`, run the following command:
```bash
./run.sh --experiment="dp_interval_vs_overhead_video"  --config_file="configs/dp_interval_vs_overhead_video.json"
```
The results will be saved in the `/results` directory. The corresponding plots are saved in the `/plots` directory.