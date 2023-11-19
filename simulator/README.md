# Traffic Shaping Simulator

## Overview

## Installing Required Packages
To install all required python packages, run the following command:
```bash
pip install -r requirements.txt
```

## Running the Simulator
To run the the simulator, with a given configuration file, run the following command:
```bash
python simulator.py --experiment <"experiment_name"> --config_file <"path/to/config file.json">
```
Alternatively, you can add the ``<"experiment name">`` to the config file, and run the following command:
```bash
python simulator.py --config_file <"path/to/config file.json">
```


## Configuration File
The config file is a json file that contains all the parameters for the traffic shaping simulation. Here is a list of parameters that can be set in the config file:
* ``"experiment"``: The name of the experiment. Currently, five experiments are supported: 
    * ``"dp_interval_vs_overhead_video"``.
    * ``"dp_interval_vs_overhead_web"``.
    * ``"number_of_flows_vs_overhead_video"``.
    * ``"number_of_flows_vs_overhead_web"``.
    * ``"empirical_privacy"``.
* ``"communication_type"``: The communication type between the client and the server:
    * ``"bidirectional"``. 
    * ``"unidirectional"``. 
* ``"data_source"``: The type of data used for the experiment:
    * ``"raw_data"``: The data is loaded from series of csv files that contain the timing and size of packets.  
    * ``"processed"``: The data is loaded from a pickle file that contains the timing and size of packets.
* ``"load_data_dir_server_to_client"``: The path to the directory that contains the data files (either processed or raw data) for the server to client communication.
* ``"load_data_dir_client_to_server"``: The path to the directory that contains the data files (either processed or raw data) for the client to server communication.
* ``"max_num_of_unique_streams"``: The maximum number of unique streams in the dataset. We need this parameter to process the data.
* ``"max_num_of_traces_per_stream"``: The maximum number of traces per stream in the dataset. We need this parameter to process the data.
* ``"filter_data"``: A boolean value that indicates whether to filter the data or not. If the value is ``true``, the simulator will filter the data based on two parameters: ``"num_of_unique_streams"`` and ``"num_of_traces_per_stream"``. If the value is ``false``, the simulator will not filter the data.
* ``"num_of_unique_streams"``: The number of unique streams to be used in the experiment. This parameter is used only if ``"filter_data"`` is set to ``true``.
* ``"num_of_traces_per_stream"``: The number of traces per stream to be used in the experiment. This parameter is used only if ``"filter_data"`` is set to ``true``.

The rest of the parameters are specific to each experiment. 
* ``transort_type``: The shaping mechanism to be used in the experiment. Currently, three shaping mechanisms are supported:
    * ``"DP_dynamic"``: **NetShaper**'s method of traffic shaping.
    * ``"DP_wPrivacy"``: Our efforts to reproduce budget absorption mechanism of the work of [Kellaris et al](https://www.vldb.org/pvldb/vol7/p1155-kellaris.pdf).
    * ``"DP_static"``: Our efforts to reproduce fpa mechansim of the work of [Zhang et al](https://www.ndss-symposium.org/wp-content/uploads/2019/02/ndss2019_07B-4_Zhang_paper.pdf).
* ``"DP_mechanism"``: The DP mechanism is used for NetShaper traffic shaping (i.e., ``"DP_dynamic"``). Currently, two mechanisms are supported and we use ``"Guassian"`` for NetShaper traffic shaping:
    * ``"Gaussian"``.
    * ``"Laplace"``.
* ``"sensitivity_server_to_client"``: The sensitivity value for the server to client communication.
* ``"sensitivity_client_to_server"``: The sensitivity value for the client to server communication.
* ``"delta_server_to_client"``: The delta value for the server to client communication.
* ``"delta_client_to_server"``: The delta value for the client to server communication.
* ``"privacy_losses_server_to_client"``: A list of privacy losses (i.e., epsilon values) for the server to client communication in multiple runs of the same experiment.
* ``"privacy_losses_client_to_server"``: A list of privacy losses (i.e., epsilon values) for the client to server communication in multiple runs of the same experiment.
* ``"min_dp_decision_server_to_client"``: The minimum value of the DP decision for the server to client communication.
* ``"min_dp_decision_client_to_server"``: The minimum value of the DP decision for the client to server communication.
* ``"static_max_dp_decision"``: This variable determines if the maximum value of the DP decision is statistically defined or not. If the value is ``true``, the maximum DP decision is the maximum rate of flows in the dataset multiplied by the maximum number of parallel flows (i.e. 1000 in our setup). If the value is ``false``, the maximum DP decision is the maximum rate of flows in the dataset multiplied by the current number of parallel flows.
* ``"privacy_window_size_server_to_client"``: The length of window for which simulator calculates the aggregated privacy loss for the server to client communication.
* ``"privacy_window_size_client_to_server"``: The length of window for which simulator calculates the aggregated privacy loss for the client to server communication.
* ``"dp_intervals_us"``: A list of pairs that contains the DP intevals for the server to client communication and the client to server communication in microsenods. Each pair contains two values: the first value is DP interval for server to client communicatoin and the second value is the corresponding client to server DP interval.