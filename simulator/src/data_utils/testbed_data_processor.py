import pandas as pd
import os
import numpy as np
import json
import pickle
import configlib
import glob
from src.data_utils.DataProcessorNew import DataProcessorNew
from src.data_utils.LogProcessor import DPDecisionProcessor, RTTProcessor



def jsons_match(dir: str) -> bool:
    peer1_json_dir = os.path.join(dir, "peer1-config.json")
    with open(peer1_json_dir, 'r') as f:
        peer1_json_dir = json.load(f)

    peer2_json_dir = os.path.join(dir, "peer2-config.json")
    with open(peer2_json_dir, 'r') as f:
        peer2_json_dir = json.load(f)

    if (peer1_json_dir["shapedSender"]["noiseMultiplier"]==           peer2_json_dir["shapedReceiver"]["noiseMultiplier"] and peer1_json_dir["shapedSender"]["sensitivity"] == peer2_json_dir["shapedReceiver"]["sensitivity"] and peer1_json_dir["shapedSender"]["DPCreditorLoopInterval"] == peer2_json_dir["shapedReceiver"]["DPCreditorLoopInterval"] and peer1_json_dir["shapedSender"]["senderLoopInterval"] == peer2_json_dir["shapedReceiver"]["senderLoopInterval"]):
        return True
    else: 
        return False 
        
    
    
def get_all_iters_expect_cache(dir: str) -> list:
    all_iters = os.listdir(dir)
    # removing the cache dir from the list of all iters
    if("cache" in all_iters):
        all_iters.remove("cache")
    return all_iters

def get_num_of_iters(dir: str) -> int:
    return len(get_all_iters_expect_cache(dir))

def get_first_iter_stream_num(dir: str) -> int:
    first_iter_dir = os.path.join(dir, "1")
    matching_path = first_iter_dir + "/*"
    list_of_all_traces_first_iter  = glob.glob(matching_path)
    return len(list_of_all_traces_first_iter)


def validate_all_iters_based_on_trace_num(dir: str, expected_num_of_trace: int) -> bool:
    all_iters = get_all_iters_expect_cache(dir)
    
    for iter in all_iters:
        list_of_all_traces_iter = glob.glob(os.path.join(dir, iter, "*.pcap"))

        if (len(list_of_all_traces_iter) != expected_num_of_trace):
            print("len of faulty list", len(list_of_all_traces_iter))
            print(iter)
            return False
    return True 


 
def get_exp_config_json(data_root_dir: str, exp_dir: str, peer: str):
    if peer == "Peer1":
        exp_config = os.path.join(data_root_dir, exp_dir, "Video-Bandwidth", 'peer1-config.json')
    elif peer == "Peer2":
        exp_config = os.path.join(data_root_dir, exp_dir, "Video-Bandwidth", 'peer2-config.json')
    if not os.path.exists(exp_config):
        raise Exception('Experiment config file does not exist')
    with open(exp_config, 'r') as f:
        exp_config_json = json.load(f)
    return exp_config_json


def get_all_data_dirs(data_root_dir: str):
    all_data_dirs = []
    files_and_dirs = os.listdir(data_root_dir)
    # print(files_and_dirs)
    for file_or_dir in files_and_dirs:
        # print(file_or_dir[:11])
        if os.path.isdir(os.path.join(data_root_dir, file_or_dir)) and ((file_or_dir[:10] == "2023-04-29") or (file_or_dir[:10] == "2023-05-01")):
            all_data_dirs.append(file_or_dir)
    print(all_data_dirs)
    return all_data_dirs

def filter_data_dirs_based_params(data_root_dir: str, 
                                num_of_unique_streams: int, 
                                num_of_traces_per_stream: int, 
                                ):
    filtered_data_dirs = []
    all_data_dirs = get_all_data_dirs(data_root_dir)
    for dir in all_data_dirs:
        client_dir = os.path.join(data_root_dir, dir, "Video-Bandwidth", "Client")
        peer1_dir =  os.path.join(data_root_dir, dir, "Video-Bandwidth", "Peer1")

        stream_num_client = get_first_iter_stream_num(client_dir)
        stream_num_peer1 = get_first_iter_stream_num(peer1_dir)
        if (stream_num_client == num_of_unique_streams and stream_num_peer1 == num_of_unique_streams):
            iter_num_client = get_num_of_iters(client_dir)
            iter_num_peer1 = get_num_of_iters(peer1_dir)
            if (iter_num_client == num_of_traces_per_stream and iter_num_peer1 == num_of_traces_per_stream):
               filtered_data_dirs.append(dir) 
        
    return filtered_data_dirs 



def get_exp_info(peer1_config_json, peer2_config_json):
    exp = {}
    exp['peer1_noise_multiplier'] = peer1_config_json['shapedSender']['noiseMultiplier']
    exp['peer1_DP_interval_length_us'] = peer1_config_json['shapedSender']['DPCreditorLoopInterval']
    exp['peer1_send_interval_length_us'] = peer1_config_json['shapedSender']['senderLoopInterval']
    exp['peer1_sensitivity'] = peer1_config_json['shapedSender']['sensitivity']
    exp['peer1_maxDecisionSize'] = peer1_config_json['shapedSender']['maxDecisionSize'] 
    exp['peer1_minDecisionSize'] = peer1_config_json['shapedSender']['minDecisionSize'] 


    exp['peer2_noise_multiplier'] = peer2_config_json['shapedReceiver']['noiseMultiplier']
    exp['peer2_DP_interval_length_us'] = peer2_config_json['shapedReceiver']['DPCreditorLoopInterval']
    exp['peer2_send_interval_length_us'] = peer2_config_json['shapedReceiver']['senderLoopInterval']
    exp['peer2_sensitivity'] = peer2_config_json['shapedReceiver']['sensitivity']
    exp['peer2_maxDecisionSize'] = peer2_config_json['shapedReceiver']['maxDecisionSize'] 
    exp['peer2_minDecisionSize'] = peer2_config_json['shapedReceiver']['minDecisionSize'] 
    return exp


def get_aggregated_data_list_parallel_streams(config: configlib.Config):
    data = []
    filtered_data_dirs = filter_data_dirs_based_params(config.data_root_dir, config.max_num_of_unique_streams, config.max_num_of_traces_per_stream)
    print(filtered_data_dirs)
    
    return None 
     
def get_aggregated_data_list(config: configlib.Config):
    
    data = []
    
    filtered_data_dirs = filter_data_dirs_based_params(config.data_root_dir, config.max_num_of_unique_streams, config.max_num_of_traces_per_stream)
    print(filtered_data_dirs)

    for dir in filtered_data_dirs:
        peer1_config_json = get_exp_config_json(config.data_root_dir, dir, "Peer1")
        peer2_config_json = get_exp_config_json(config.data_root_dir, dir, "Peer2")
        exp_root_dir = os.path.join(config.data_root_dir, dir, "Video-Bandwidth")
        peer2_dir = os.path.join(exp_root_dir, "Peer2")
        client_dir = os.path.join(exp_root_dir, "Client")

        if os.path.exists(os.path.join(peer2_dir, "cache", "Peer2", "shaped_data.pickle")):
            with open(os.path.join(peer2_dir, "cache", "Peer2", "shaped_data.pickle"), 'rb') as f:
                shaped_data = pickle.load(f) 
        # loading the shaped data from raw data
        else: 
            tmp_dp_peer2 = DataProcessorNew(peer2_dir, config.max_num_of_unique_streams, config.max_num_of_traces_per_stream)
            shaped_data = tmp_dp_peer2.aggregated_dataframe(config.data_time_resolution_us) 
            
            # Save the shaped data to cache
            os.makedirs(os.path.join(peer2_dir, "cache", "Peer1"), exist_ok=True)
            with open(os.path.join(peer2_dir, "cache", "Peer1", "shaped_data.pickle"), 'wb') as f:
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


        exp = get_exp_info(peer1_config_json, peer2_config_json)


       
        exp['shaped_data'] = shaped_data
        exp['unshaped_data'] = unshaped_data
        # print(exp['noise_multiplier'])
        data.append(exp)
    # for d in data:
    #     print(d['noise_multiplier'])
    return data        


def get_aggregated_sizes_dp_decisions(config: configlib.Config):
    aggregated_sizes_dp_decisions_list = []
    filtered_log_dirs = filter_data_dirs_based_params(config.log_root_dir, config.max_num_of_unique_streams, config.max_num_of_traces_per_stream) 
    
    for dir in filtered_log_dirs:
        peer1_config_json = get_exp_config_json(config.log_root_dir, dir, "Peer1")
        peer2_config_json = get_exp_config_json(config.log_root_dir, dir, "Peer2")
        exp_root_dir = os.path.join(config.log_root_dir, dir, "Video-Bandwidth")
        peer1_dir = os.path.join(exp_root_dir, "Peer1")
        peer2_dir = os.path.join(exp_root_dir, "Peer2")

        # if os.path.exists(os.path.join(peer1_dir, "cache", "Peer1", "aggregated_sizes_DP_decisions.pickle")):
        if False:
            with open(os.path.join(peer1_dir, "cache", "Peer1", "aggregated_sizes_DP_decisions.pickle"), 'rb') as f:
                aggregated_sizes_DP_decisions_peer1 = pickle.load(f)
        else:
            DPp_peer1 = DPDecisionProcessor(peer1_dir, config.max_num_of_traces_per_stream)
            aggregated_sizes_DP_decisions_peer1 = DPp_peer1.get_size_and_DP_decisions()

            # save results to the cache
            os.makedirs(os.path.join(peer1_dir, "cache", "Peer1"), exist_ok=True)
            with open(os.path.join(peer1_dir, "cache", "Peer1", "aggregated_sizes_DP_decisions.pickle"), 'wb') as f:
                pickle.dump(aggregated_sizes_DP_decisions_peer1, f)

        # if os.path.exists(os.path.join(peer2_dir, "cache", "Peer2", "aggregated_sizes_DP_decisions.pickle")):
        if False:
            with open(os.path.join(peer2_dir, "cache", "Peer2", "aggregated_sizes_DP_decisions.pickle"), 'rb') as f:
                aggregated_sizes_DP_decisions_peer2 = pickle.load(f)
        else: 
            DPp_peer2 = DPDecisionProcessor(peer2_dir, config.max_num_of_traces_per_stream)
            aggregated_sizes_DP_decisions_peer2 = DPp_peer2.get_size_and_DP_decisions() 

            
            # Save results to the cache 
            os.makedirs(os.path.join(peer2_dir, "cache", "Peer2"), exist_ok=True)
            with open(os.path.join(peer2_dir, "cache", "Peer2", "aggregated_sizes_DP_decisions.pickle"), 'wb') as f:
                pickle.dump(aggregated_sizes_DP_decisions_peer2, f)

        exp = get_exp_info(peer1_config_json, peer2_config_json)

        exp['aggregated_sizes_DP_decisions_peer1'] = aggregated_sizes_DP_decisions_peer1
        exp['aggregated_sizes_DP_decisions_peer2'] = aggregated_sizes_DP_decisions_peer2
        
        aggregated_sizes_dp_decisions_list.append(exp)
    
    return aggregated_sizes_dp_decisions_list


def get_aggregated_RTTs(config: configlib.Config):
    RTTs_list = []
    filtered_log_dirs = filter_data_dirs_based_params(config.log_root_dir, config.max_num_of_unique_streams, config.max_num_of_traces_per_stream)
    
    for dir in filtered_log_dirs:
        peer1_config_json = get_exp_config_json(config.log_root_dir, dir, "Peer1")
        peer2_config_json = get_exp_config_json(config.log_root_dir, dir, "Peer2")
        exp_root_dir = os.path.join(config.log_root_dir, dir, "Video-Bandwidth")
        client_dir = os.path.join(exp_root_dir, "Client")
        
        # if os.path.exists(os.path.join(client_dir, "cache", "Clinet", "RTTs.pickle")):
        if False:
            with open(os.path.join(client_dir, "cache", "Client", "RTTs.pickle"), 'rb') as f:
                RTTs = pickle.load(f)
        else: 
            RTTp_client = RTTProcessor(client_dir, config.max_num_of_traces_per_stream)
            RTTs = RTTp_client.get_RTTs() 
            
            # Save results to the cache
            # os.makedirs(os.path.join(client_dir, "cache", "Clinet"), exist_ok=True)
            # with open(os.path.join(client_dir, "cache", "Client", "RTTs.pickle"), 'wb') as f:
            #     pickle.dump(RTTs, f)

        exp = get_exp_info(peer1_config_json, peer2_config_json)
        # print(exp['peer1_sensitivity'])
        if (len(RTTs) <= 1000):
            continue
        exp['RTTs'] = RTTs
        
        RTTs_list.append(exp)
    return RTTs_list




def get_data_aggregator_function(config: configlib.Config):
    if config.analysis_type == "privacy":
        return get_aggregated_data_list
    elif config.analysis_type == "overhead":
        return get_aggregated_data_list
    elif config.analysis_type == "dp_decisions":
        return get_aggregated_sizes_dp_decisions
    elif config.analysis_type == "latency":
        return get_aggregated_RTTs 
    elif config.analysis_type == "number_of_traces_vs_overhead":
        return get_aggregated_data_list_parallel_streams
    else: 
        raise Exception("Invalid analysis type, please specify analysis_type in the config file")