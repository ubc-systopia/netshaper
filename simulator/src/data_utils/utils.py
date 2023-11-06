import pickle
import pandas as pd
import os
import numpy as np
import datetime
from src.data_utils.DataProcessor import DataProcessor
from src.data_utils.DataProcessorNew import DataProcessorNew
import configlib


def data_loader(config:configlib.Config):
    if config.communication_type == "unidirectional":
        return (data_loader_unidirectional(
                                       config.load_data_dir_server_to_client, config.data_source,
                                       config.setup_version,
                                       config.data_time_resolution_server_to_client_us,
                                       config.max_num_of_unique_streams,
                                       config.max_num_of_traces_per_stream), None)
    elif config.communication_type == "bidirectional":
        return (
            data_loader_unidirectional(config.load_data_dir_server_to_client,
                                       config.data_source,
                                       config.setup_version,
                                       config.data_time_resolution_server_to_client_us,
                                       config.max_num_of_unique_streams,
                                       config.max_num_of_traces_per_stream),
            data_loader_unidirectional(config.load_data_dir_client_to_server,
                                       config.data_source,
                                       config.setup_version,
                                       config.data_time_resolution_client_to_server_us,
                                       config.max_num_of_unique_streams,
                                       config.max_num_of_traces_per_stream)
            )
    else:
        raise ValueError("Please specify the communication type you want to run. ('unidirectional' or 'bidirectional')")



def data_loader_unidirectional(load_data_dir: str, data_source: str, setup_version: str, data_time_resolution_us: int, max_num_of_unique_streams: int, max_num_of_traces_per_stream: int):
    if load_data_dir is None:
        raise ValueError("Please specify the directory of the dataset you want to load.")

    if data_source == "memory":
        file_name = 'data_trace_w_' + str(int(data_time_resolution_us)) + '_c_'+ str(int(max_num_of_unique_streams)) + '.p'
        file_dir = os.path.join(load_data_dir, file_name)
        df = pickle.load(open(file_dir, "rb"))

    elif data_source == "raw_data":
        if setup_version == "v1":
            dp = DataProcessor(load_data_dir, max_num_of_unique_streams, [1, 10], ['pkt_time', 'pkt_size'])
        elif setup_version == "v2":
            dp = DataProcessorNew(load_data_dir, max_num_of_unique_streams, max_num_of_traces_per_stream)
        else:
            raise ValueError("Please specify the setup you want to run. ('v1' or 'v2')")
        print(data_time_resolution_us)
        df = dp.aggregated_dataframe(data_time_resolution_us)
        # Save a backup in /tmp/ for future use
        pickle.dump(df, open('/tmp/data_trace_w_' + str(int(data_time_resolution_us)) + '_c_'+ str(int(max_num_of_unique_streams)) + '.p', "wb"))

    else: 
        raise ValueError("Please specify the data source you want to load from. ('memory' or 'raw_data')")

    return df
        
        
        
def data_saver(df, data_dir, data_time_resolution_us, num_classes):
    ensure_dir(data_dir)
    data_path = os.path.join(data_dir, 'data_trace_w_' + str(int(data_time_resolution_us)) + '_c_'+ str(int(num_classes)) + '.p')
    pickle.dump(df, open(data_path, "wb"))

def data_saver(data, config):
    ensure_dir(config.save_data_dir)
    server_to_client_dir = os.path.join(config.save_data_dir, "server_to_client")
    print(server_to_client_dir)
    ensure_dir(server_to_client_dir)
    file_name = os.path.join(server_to_client_dir, 'data_trace_w_' + str(int(config.data_time_resolution_server_to_client_us)) + '_c_'+ str(int(config.max_num_of_unique_streams)) + '.p')
    pickle.dump(data[0], open(file_name, "wb"))

    if config.communication_type == "bidirectional":
        client_to_server_dir = os.path.join(config.save_data_dir, "client_to_server")
        ensure_dir(client_to_server_dir) 
        file_name = os.path.join(client_to_server_dir, 'data_trace_w_' + str(int(config.data_time_resolution_client_to_server_us)) + '_c_'+ str(int(config.max_num_of_unique_streams)) + '.p')
        pickle.dump(data[1], open(file_name, "wb"))    
        
def data_filter_stochastic(config: configlib.Config, df):

    num_of_unique_streams = config.num_of_unique_streams
    max_num_of_unique_streams = config.max_num_of_unique_streams
    num_of_traces_per_stream = config.num_of_traces_per_stream
    max_num_of_traces_per_stream = config.max_num_of_traces_per_stream


    random_labels = np.sort(np.random.choice(range(max_num_of_unique_streams), num_of_unique_streams, replace=False))
    random_labels_new = np.array(range(num_of_unique_streams))
    print(random_labels)
    # Filter the dataframe based on the random labels
    filtered_df = df[df['label'].isin(random_labels)] 
    
    # From every unique label, select a num_of_traces_per_stream number of traces
    filtered_df = filtered_df.groupby('label').apply(lambda x: x.sample(n=num_of_traces_per_stream, replace=False)) 
    
    # map the labels to the new labels
    # print(filtered_df.head)
    filtered_df = filtered_df.replace(random_labels, random_labels_new).reset_index(drop=True)

    return filtered_df

def data_filter_deterministic(config: configlib.Config, data):

    num_of_unique_streams = config.num_of_unique_streams
    max_num_of_unique_streams = config.max_num_of_unique_streams
    num_of_traces_per_stream = config.num_of_traces_per_stream
    max_num_of_traces_per_stream = config.max_num_of_traces_per_stream

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
        
    return (filtered_server_to_client_df, filtered_client_to_server_df)


def experiment_result_saver(config: configlib.Config, results, baseline_results = None):
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
    
    # Save the baseline results if there is any
    if baseline_results is not None:
        with open(child_dir + "baseline_results.pkl", "wb") as f: 
            pickle.dump(baseline_results, f)
            

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
    if config.filtering_type == 'deterministic':
        return data_filter_deterministic
    elif config.filtering_type == 'stochastic':
        return data_filter_stochastic
    elif config.filtering_type is None:
        return lambda x: x 
    else:
        raise NotImplementedError("Filtering type not implemented")
    
    
def prune_data(config: configlib.Config, data):
   if (config.experiment == "noise_multiplier_vs_overhead_video" or config.experiment == "number_of_traces_vs_overhead_video"):
       return data
   else:

        server_to_client_data = data[0]
        client_to_server_data = data[1]
        if config.communication_type == "bidirectional":
            return (server_to_client_data.drop(columns = ['video_name']), client_to_server_data.drop(columns = ['video_name'])) 
        elif config.communication_type == "unidirectional":
            return (server_to_client_data.drop(columns = ['video_name']), None) 