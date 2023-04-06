import pandas as pd
import os
import numpy as np
import json
import pickle
import configlib

from src.data_utils.DataProcessorNew import DataProcessorNew


def get_exp_config_json(data_root_dir: str, exp_dir: str):
    exp_config = os.path.join(data_root_dir, exp_dir, 'configs.json')
    if not os.path.exists(exp_config):
        raise Exception('Experiment config file does not exist')
    with open(exp_config, 'r') as f:
        exp_config_json = json.load(f)
    return exp_config_json


def get_all_data_dirs(data_root_dir: str):
    all_data_dirs = []
    files_and_dirs = os.listdir(data_root_dir)
    for file_or_dir in files_and_dirs:
        if os.path.isdir(os.path.join(data_root_dir, file_or_dir)):
            all_data_dirs.append(file_or_dir)
    return all_data_dirs

def filter_data_dirs_based_params(data_root_dir: str, 
                                num_of_unique_streams: int, 
                                num_of_traces_per_stream: int):
    filtered_data_dirs = []
    all_data_dirs = get_all_data_dirs(data_root_dir)
    for dir in all_data_dirs:
        exp_config_json = get_exp_config_json(data_root_dir, dir)

        if exp_config_json['dataset']['videoNum'] == num_of_unique_streams and exp_config_json['dataset']['iterNum'] == num_of_traces_per_stream:
            filtered_data_dirs.append(dir)
    
    return filtered_data_dirs 

def aggregated_data_list(config: configlib.Config):
    data = []
    filtered_data_dirs = filter_data_dirs_based_params(config.data_root_dir, config.max_num_of_unique_streams, config.max_num_of_traces_per_stream)
    for dir in filtered_data_dirs:
        exp = {}
        exp_config_json = get_exp_config_json(config.data_root_dir, dir)
        exp_root_dir = os.path.join(config.data_root_dir, dir, "Video-Bandwidth")
        peer1_dir = os.path.join(exp_root_dir, "Peer1")
        client_dir = os.path.join(exp_root_dir, "Client")

        if os.path.exists(os.path.join(peer1_dir, "cache", "Peer1", "shaped_data.pickle")):
            with open(os.path.join(peer1_dir, "cache", "Peer1", "shaped_data.pickle"), 'rb') as f:
                shaped_data = pickle.load(f) 
        # loading the shaped data from raw data
        else: 
            tmp_dp_peer1 = DataProcessorNew(peer1_dir, config.max_num_of_unique_streams, config.max_num_of_traces_per_stream)
            shaped_data = tmp_dp_peer1.aggregated_dataframe(config.data_time_resolution_us) 
            
            # Save the shaped data to cache
            os.makedirs(os.path.join(peer1_dir, "cache", "Peer1"), exist_ok=True)
            with open(os.path.join(peer1_dir, "cache", "Peer1", "shaped_data.pickle"), 'wb') as f:
                pickle.dump(shaped_data, f)
        # loading the unshaped data from cache
        if os.path.exists(os.path.join(client_dir, "cache", "Client", "unshaped_data.pickle")):
            with open(os.path.join(client_dir, "cache", "Client", "unshaped_data.pickle"), 'rb') as f:
                unshaped_data = pickle.load(f) 
        # loading the unshaped data from raw data
        else: 
            tmp_dp_client = DataProcessorNew(client_dir, config.max_num_of_unique_streams, config.max_num_of_traces_per_stream)
            unshaped_data = tmp_dp_client.aggregated_dataframe(config.data_time_resolution_us)
            
            # Save the unshaped data to cache
            os.makedirs(os.path.join(client_dir, "cache", "Client"), exist_ok=True)
            with open(os.path.join(client_dir, "cache", "Client", "unshaped_data.pickle"), 'wb') as f:
                pickle.dump(unshaped_data, f)  



        exp['noise_multiplier'] = exp_config_json['peer1']['noiseMultiplier']
        exp['DP_interval_length_us'] = exp_config_json['peer1']['DPCreditorLoopInterval']
        exp['send_interval_length_us'] = exp_config_json['peer1']['senderLoopInterval']
        exp['shaped_data'] = shaped_data
        exp['unshaped_data'] = unshaped_data
        # print(exp['noise_multiplier'])
        data.append(exp)
    # for d in data:
    #     print(d['noise_multiplier'])
    return data        



if __name__ == "__main__":
    config = configlib.Config()
    data = aggregated_data(config)
    print(data)