import pickle
import pandas as pd
import os
import numpy as np
from src.data_utils.DataProcessor import DataProcessor



def data_loader(data_time_resolution_us, data_dir = None, load_from = 'raw_data', num_classes = 4):
  if(data_dir == None):
    print("Please specify the directory of the dataset you want to load.")
    df = None
  if(load_from == 'memory'):
    file_dir = data_dir + 'data_trace_w_' + str(int(data_time_resolution_us)) + '_c_'+ str(int(num_classes)) + '.p'
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

def data_saver(df, data_dir, data_time_resolution_us, num_classes):
  data_path = os.path.join(data_dir, 'data_trace_w_' + str(int(data_time_resolution_us)) + '_c_'+ str(int(num_classes)) + '.p')
  pickle.dump(df, open(data_path, "wb"))
  

def data_filter_stochastic(df, num_of_unique_streams, num_of_traces_per_stream,max_num_of_unique_streams, max_num_of_traces_per_stream):

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

def data_filter_deterministic(df, num_of_unique_streams, num_of_traces_per_stream,max_num_of_unique_streams, max_num_of_traces_per_stream):
  # Filter the dataframe based on the random labels
  filtered_df = df[df['label'] < num_of_unique_streams] 
  
  # From every unique label, select a num_of_traces_per_stream number of traces
  filtered_df = filtered_df.groupby('label').apply(lambda x: x.sample(n=num_of_traces_per_stream, replace=False)) 
  
  # map the labels to the new labels
  # print(filtered_df.head)
  filtered_df = filtered_df.reset_index(drop=True)

  return filtered_df