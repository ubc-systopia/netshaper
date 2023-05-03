import pickle
import pandas as pd
import os
import numpy as np
import datetime
from src.data_utils.DataProcessor import DataProcessor
from src.data_utils.DataProcessorNew import DataProcessorNew
import configlib


def data_loader_experimental(data_time_resolution_us, data_dir = None, load_from = 'raw_data', num_classes = 4):
    if(data_dir == None):
        print("Please specify the directory of the dataset you want to load.")
        df = None
    if(load_from == 'memory'):
        file_name = 'data_trace_w_' + str(int(data_time_resolution_us)) + '_c_'+ str(int(num_classes)) + '.p'
        file_dir = os.path.join(data_dir, file_name)
        df = pickle.load(open(file_dir, "rb"))
    elif(load_from == 'raw_data'):
        dp = DataProcessor(data_dir, num_classes, [1, 10], ['pkt_time', 'pkt_size'])
        df = dp.aggregated_dataframe(data_time_resolution_us) 
        # Saving the dataframe in /tmp/ for future use
        pickle.dump(df, open('/tmp/data_trace_w_' + str(int(data_time_resolution_us)) + '_c_'+ str(int(num_classes)) + '.p', "wb"))
    else:
        print("Please specify the data source you want to load from. ('memory' or 'raw_data')")
        df = None
    return df

def data_loader_realistic(data_time_resolution_us, data_dir = None, load_from = 'raw_data', num_classes = 4, iter_num = 10):
    if(data_dir == None):
        print("Please specify the directory of the dataset you want to load.")
        df = None
    if(load_from == 'memory'):
        file_name = 'data_trace_w_' + str(int(data_time_resolution_us)) + '_c_'+ str(int(num_classes)) + '.p'
        file_dir = os.path.join(data_dir, file_name)
        df = pickle.load(open(file_dir, "rb"))
    elif(load_from == 'raw_data'):
        print(num_classes)
        dp = DataProcessorNew(data_dir, num_classes, iter_num)
        df = dp.aggregated_dataframe(data_time_resolution_us) 
        # Saving the dataframe in /tmp/ for future use
        pickle.dump(df, open('/tmp/data_trace_w_' + str(int(data_time_resolution_us)) + '_c_'+ str(int(num_classes)) + '.p', "wb"))
    else:
        print("Please specify the data source you want to load from. ('memory' or 'raw_data')")
        df = None
    return df
def data_saver(df, data_dir, data_time_resolution_us, num_classes):
    ensure_dir(data_dir)
    data_path = os.path.join(data_dir, 'data_trace_w_' + str(int(data_time_resolution_us)) + '_c_'+ str(int(num_classes)) + '.p')
    pickle.dump(df, open(data_path, "wb"))


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

def data_filter_deterministic(config: configlib.Config, df):
    num_of_unique_streams = config.num_of_unique_streams
    max_num_of_unique_streams = config.max_num_of_unique_streams
    num_of_traces_per_stream = config.num_of_traces_per_stream
    max_num_of_traces_per_stream = config.max_num_of_traces_per_stream

    # print(type(num_of_unique_streams))
    # print(type(df['label'][0])) 
    # Filter the dataframe based on the random labels
    filtered_df = df[df['label'] < num_of_unique_streams] 
    
    # From every unique label, select a num_of_traces_per_stream number of traces
    # print(num_of_traces_per_stream)
    filtered_df = filtered_df.groupby('label').apply(lambda x: x.sample(n=num_of_traces_per_stream, replace=False)) 
    
    # map the labels to the new labels
    # print(filtered_df.head)
    filtered_df = filtered_df.reset_index(drop=True)

    return filtered_df


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
    if config.filtering_type == 'stochastic':
        return data_filter_stochastic
    else:
        raise NotImplementedError("Filtering type not implemented")
    
    
def prune_data(config: configlib.Config, df):
   if (config.experiment == "noise_multiplier_vs_overhead_video" or config.experiment == "number_of_traces_vs_overhead_video"):
       return df
   else:
       tmp_df = df.drop(columns = ['video_name'])
       return tmp_df 