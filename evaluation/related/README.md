# NetShaper vs Other Shaping Mechanisms
This directory contains the code for the NetShaper vs Other Shaping Mechanisms experiment. 
We compare NetShaper with [Pacer](https://www.usenix.org/system/files/sec22-mehta.pdf) and Constant-Rate Shaping.


## Prerequisites
To install the python dependencies, run the following command:
```bash
pip install -r ../../simulator/requirements.txt
```
## Configuration
```json
{
  "data_source": "processed",
  "load_data_dir_server_to_client": "../../dataset/simulator/video/cache/server_to_client/",
  "load_data_dir_client_to_server": "../../dataset/simulator/video/cache/client_to_server/",
  "pacer_data_dir": "../../dataset/simulator/video/pacer/tvseries_720p_sizes.csv",
  "results_dir": "results/",
  "plot_dir": "plots/",  
  "max_num_of_unique_streams": 100,
  "max_num_of_traces_per_stream": 100,

  "experiment": "overhead_comparison_video",
  "communication_type": "bidirectional",
  "num_of_unique_streams": 100,
  "num_of_traces_per_stream": 100,
  "filter_data": "true",


  "transport_type": "DP_dynamic",
  "DP_mechanism" : "Gaussian",
  "min_video_num": 100,
  "max_video_num": 1000,
  "num_of_video_nums": 10,
  "max_videos_per_single_experiment": 10,
  "privacy_losses_server_to_client": [1, 4, 16], 
  "privacy_losses_client_to_server": [1, 4, 16],
  "delta_server_to_client": 1e-6,
  "delta_client_to_server": 1e-6,
  "sensitivity_server_to_client": 2.5e6,
  "sensitivity_client_to_server": 200,
  "min_dp_decision_server_to_client": 0,
  "min_dp_decision_client_to_server": 0,
  "static_max_dp_decision": true,
  "dp_intervals_us": [[1e6, 1e4]] ,
  "privacy_window_size_server_to_client_s": 5,
  "privacy_window_size_client_to_server_s": 1
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
16. `min_video_num`: The minimum number of videos to be used in the experiment.
17. `max_video_num`: The maximum number of videos to be used in the experiment.
18. `num_of_video_nums`: The number of different number of videos to be used in the experiment (for example if `min_video_num` is 1, `max_video_num` is 8, and `num_of_video_nums` is 4, the number of parallel videos would be the [1, 3, 5, 7]).
19. `max_videos_per_single_experiment`: The number of repeated experiments for each number of videos.
20. `privacy_losses_server_to_client`: A list of privacy losses (i.e., epsilon values) for the server to client communication in multiple runs of the same experiment.
21. `privacy_losses_client_to_server`: A list of privacy losses (i.e., epsilon values) for the client to server communication in multiple runs of the same experiment.
22. `delta_server_to_client`: The delta value for the server to client communication.
23. `delta_client_to_server`: The delta value for the client to server communication.
24. `sensitivity_server_to_client`: The sensitivity value for the server to client communication.
25. `sensitivity_client_to_server`: The sensitivity value for the client to server communication.
26. `min_dp_decision_server_to_client`: The minimum value of the DP decision for the server to client communication.
27. `min_dp_decision_client_to_server`: The minimum value of the DP decision for the client to server communication.
28. `static_max_dp_decision`: This variable determines if the maximum value of the DP decision is statistically defined or not. If the value is `true`, the maximum DP decision is the maximum rate of flows in the dataset multiplied by the maximum number of parallel flows (i.e. 1000 in our setup). If the value is `false`, the maximum DP decision is the maximum rate of flows in the dataset multiplied by the current number of parallel flows.
29. `dp_intervals_us`: A pair of DP intervals for the server to client communication and the client to server communication in microseconds. Note that the first value is DP interval for server to client communication and the second value is the corresponding client to server DP interval.
30. `privacy_window_size_server_to_client_s`: The length of window for which simulator calculates the aggregated privacy loss for the server to client communication.
31. `privacy_window_size_client_to_server_s`: The length of window for which simulator calculates the aggregated privacy loss for the client to server communication.


## Run
To run compare bandwidth overhead of NetShaper with Pacer and Constant-Rate Shaping for the web dataset, run the following command:
```bash
./run.sh --experiment="overhead_comparison_web"  --config_file="configs/overhead_comparison_web.json"
```
To run the experiment for comparing video overheads, run the following command:
```bash
./run.sh --experiment="overhead_comparison_video"  --config_file="configs/overhead_comparison_video.json"
```
The results will be saved in the `/results` directory. The corresponding plots are saved in the `/plots` directory.