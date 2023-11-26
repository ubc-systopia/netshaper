import pickle
import pandas as pd
import os
import numpy as np
import datetime
from src.data_utils.DataProcessor import DataProcessor
from src.data_utils.DataProcessorNew import DataProcessorNew
import configlib


def get_data_loader_fn(config: configlib.Config):
    if config.experiment == "privacy_loss_vs_noise_std" or config.experiment == "privacy_loss_vs_query_num":
        return lambda x: None
    else:
        return data_loader

def get_data_saver_fn(config: configlib.Config):
    if config.experiment == "privacy_loss_vs_noise_std" or config.experiment == "privacy_loss_vs_query_num":
        return lambda x, y: None
    else: 
        return data_saver

def get_data_pruner_fn(config: configlib.Config):
    if config.experiment == "privacy_loss_vs_noise_std" or config.experiment == "privacy_loss_vs_query_num":
        return lambda x, y: y
    else: 
        return data_prune

def data_loader(config:configlib.Config):
    data = []
    for dp_interval in config.dp_intervals_us:
        dp_interval_s_to_c_us, dp_interval_c_to_s_us = dp_interval[0], dp_interval[1]
        if config.communication_type == "unidirectional":
            data.append((data_loader_unidirectional(
                                        config.load_data_dir_server_to_client, config.data_source,
                                        dp_interval_s_to_c_us,
                                        config.max_num_of_unique_streams,
                                        config.max_num_of_traces_per_stream), None))
        elif config.communication_type == "bidirectional":
            data.append(
                (data_loader_unidirectional(config.load_data_dir_server_to_client,
                                        config.data_source,
                                        dp_interval_s_to_c_us,
                                        config.max_num_of_unique_streams,
                                        config.max_num_of_traces_per_stream),
                data_loader_unidirectional(config.load_data_dir_client_to_server,
                                        config.data_source,
                                        dp_interval_c_to_s_us,
                                        config.max_num_of_unique_streams,
                                        config.max_num_of_traces_per_stream))
                )
        else:
            raise ValueError("Please specify the communication type you want to run. ('unidirectional' or 'bidirectional')")
    return data


def data_loader_unidirectional(load_data_dir: str, data_source: str, data_time_resolution_us: int, max_num_of_unique_streams: int, max_num_of_traces_per_stream: int):
    if load_data_dir is None:
        raise ValueError("Please specify the directory of the dataset you want to load.")

    if data_source == "processed":
        file_name = 'data_trace_w_' + str(int(data_time_resolution_us)) + '_c_'+ str(int(max_num_of_unique_streams)) + '.p'
        file_dir = os.path.join(load_data_dir, file_name)
        df = pickle.load(open(file_dir, "rb"))

    elif data_source == "raw_data":
        dp = DataProcessorNew(load_data_dir, max_num_of_unique_streams, max_num_of_traces_per_stream)
        print(data_time_resolution_us)
        df = dp.aggregated_dataframe(data_time_resolution_us)
        # Save a backup in /tmp/ for future use
        pickle.dump(df, open('/tmp/data_trace_w_' + str(int(data_time_resolution_us)) + '_c_'+ str(int(max_num_of_unique_streams)) + '.p', "wb"))

    else: 
        raise ValueError("Please specify the data source you want to load from. ('memory' or 'raw_data')")

    return df
        
        
        

def data_saver(config, data_list):
    for index, dp_interval in enumerate(config.dp_intervals_us):
        dp_interval_s_to_c_us, dp_interval_c_to_s_us = dp_interval[0], dp_interval[1]
        ensure_dir(config.save_data_dir)
        server_to_client_dir = os.path.join(config.save_data_dir, "server_to_client")
        ensure_dir(server_to_client_dir)
        file_name = os.path.join(server_to_client_dir, 'data_trace_w_' + str(int(dp_interval_s_to_c_us)) + '_c_'+ str(int(config.max_num_of_unique_streams)) + '.p')
        pickle.dump(data_list[index][0], open(file_name, "wb"))

        if config.communication_type == "bidirectional":
            client_to_server_dir = os.path.join(config.save_data_dir, "client_to_server")
            ensure_dir(client_to_server_dir) 
            file_name = os.path.join(client_to_server_dir, 'data_trace_w_' + str(int(dp_interval_c_to_s_us)) + '_c_'+ str(int(config.max_num_of_unique_streams)) + '.p')
            pickle.dump(data_list[index][1], open(file_name, "wb"))    
                

def data_filter_deterministic(config: configlib.Config, data_list):
    filtered_data_list = []
    for data in data_list:
        num_of_unique_streams = config.num_of_unique_streams
        num_of_traces_per_stream = config.num_of_traces_per_stream

        # print(type(num_of_unique_streams))
        # print(type(df['label'][0])) 

        server_to_client_df = data[0]

        # print(type(server_to_client_df))
        filtered_server_to_client_df= server_to_client_df[server_to_client_df['label'] < num_of_unique_streams] 

        filtered_server_to_client_df = filtered_server_to_client_df.groupby('label').apply(lambda x: x.sample(n=num_of_traces_per_stream, replace=False))


        filtered_server_to_client_df = filtered_server_to_client_df.reset_index(drop=True)


        if config.communication_type == "bidirectional":
            client_to_server_df = data[1] 
        
        
            filtered_client_to_server_df= client_to_server_df[client_to_server_df['label'] < num_of_unique_streams]
        
        
            filtered_client_to_server_df = filtered_client_to_server_df.groupby('label').apply(lambda x: x.sample(n=num_of_traces_per_stream, replace=False)) 
        
        
        
            # map the labels to the new labels
            # print(filtered_df.head)
            filtered_client_to_server_df = filtered_client_to_server_df.reset_index(drop=True)

        else:  
            filtered_client_to_server_df = None 
            
        filtered_data_list.append((filtered_server_to_client_df, filtered_client_to_server_df))

    return filtered_data_list


def get_result_saver_fn(config: configlib.Config):
    if config.experiment == "privacy_loss_vs_noise_std" or config.experiment == "privacy_loss_vs_query_num":
        return lambda x, y: None
    else:
        return experiment_result_saver





def experiment_result_saver(config: configlib.Config, results):
    if not os.path.exists(config.results_dir):
        os.mkdir(config.results_dir)

    # Create a directory based on the experiment date and time
    now = datetime.datetime.now()
    child_dir =  config.results_dir + config.experiment + "_(" + now.strftime("%Y-%m-%d_%H-%M") + ")/" 
    os.mkdir(child_dir)
    
    # save the config file in the results directory if results 
    with open(child_dir + "config.json", "w") as f: 
        f.write(str(config)) 
    
    # Save the results 
    with open(child_dir + "results.pkl", "wb") as f:
        pickle.dump(results, f)
           

def analysis_result_saver(config: configlib.Config, results):
    analysis_dir = os.path.join(config.results_dir, "data_analysis")
    ensure_dir(config.results_dir)
    now = datetime.datetime.now()
    child_dir = os.path.join(analysis_dir, config.analysis_type + "_(" + now.strftime("%Y-%m-%d_%H-%M") + ")")
    os.makedirs(child_dir) 

    with open(child_dir + "/results.pkl", "wb") as f:
        pickle.dump(results, f)
            
def ensure_dir(file_path):
    directory = os.path.dirname(file_path)
    if not os.path.exists(directory):
        os.makedirs(directory)
        
def get_data_filter_function(config):
    if config.experiment == "privacy_loss_vs_noise_std" or config.experiment == "privacy_loss_vs_query_num":
        return lambda x, y: None
    else:
        if config.filter_data:
            return data_filter_deterministic
        else:
            return lambda x, y: y 

    
def data_prune(config: configlib.Config, data_list):
   if (config.experiment == "noise_multiplier_vs_overhead_video" or config.experiment == "number_of_flows_vs_overhead_video"):
       return data_list
   else:
        pruned_data_list = [] 
        for data in data_list:
            server_to_client_data = data[0]
            client_to_server_data = data[1]
            if config.communication_type == "bidirectional":
                pruned_data_list.append(
                    (server_to_client_data.drop(columns = ['video_name']), client_to_server_data.drop(columns = ['video_name']))
                ) 
            elif config.communication_type == "unidirectional":
                pruned_data_list.append(
                    (server_to_client_data.drop(columns = ['video_name']), None)
                ) 
        return pruned_data_list
