import pandas as pd
import os
import math
from tqdm import tqdm

class DataProcessor():
  def __init__(self, dir, num_of_classes, useful_columns_indices, useful_columns_names):
    self.directory = dir
    self.num_of_classes = num_of_classes
    self.useful_columns_names = useful_columns_names
    self.useful_columns_indices = useful_columns_indices

  def partition_files_base_filename(self):
    files_in_dir = os.listdir(self.directory)
    partioned_ds_names = {}
    for label in range(self.num_of_classes):
      key = 'v' + str(label)
      tmp_values = []
      for file in files_in_dir:
        if (key + '.tshark') in file:
          tmp_values.append(file) 
      partioned_ds_names[str(label)] = tmp_values
    return partioned_ds_names


  def make_dataset_dataframe(self, dataset_name):
    if (len(self.useful_columns_indices) != len(self.useful_columns_names)):
      return -1
    dataset_dir = os.path.join(self.directory, dataset_name) 
    df = pd.read_csv(dataset_dir, delimiter = "\t", header=None)
    df = df.iloc[:,self.useful_columns_indices]
    df.columns = self.useful_columns_names
    return df

  # here we use binary search to find index of a timestamp. In fact, we are 
  # exploiting the fact that timestamps are sorted.
  def binary_search(self, df, value, column_to_search):
    lower_ind = 0
    upper_ind = len(df)-1
    while(not (df.iloc[upper_ind, column_to_search] >= value and df.iloc[upper_ind-1, column_to_search] <= value)):
        middle_ind = math.floor((upper_ind + lower_ind)/2)
        if(df.iloc[middle_ind, column_to_search] < value):
            lower_ind = middle_ind + 1
        else:
            upper_ind = middle_ind
    return upper_ind


  # This function take two timestamps as start and end of a window and returns the 
  # amount of traffic transimted inside that window time in Bytes.
  def burst_in_window_size(self, df, window_start_time, window_end_time, pkt_size_column_indx, pkt_time_column_indx):
    column_to_search_indx = pkt_time_column_indx
    start_window_indx = self.binary_search(df, window_start_time, column_to_search_indx)
    end_window_indx = self.binary_search(df, window_end_time, column_to_search_indx)
    burst_in_window = df.iloc[start_window_indx:end_window_indx,  pkt_size_column_indx].sum()
    return burst_in_window



  # In developement of this function I assumed the data set has the following format
  #   ['index', 'pkt_time', pkt_size']
  # Caution: window_size is in uS so we need to be sure that pkt_time is in uS
  def burst_in_window_pattern(self, df, window_size):

    # Convert second metric to microsecond for all
    df['pkt_time']  = df['pkt_time'] * 1e6

    pkt_time_column_indx = df.columns.get_loc('pkt_time')
    pkt_size_column_indx = df.columns.get_loc('pkt_size')

    stream_start_time = df.iloc[0, pkt_time_column_indx]
    stream_end_time = df.iloc[-1, pkt_time_column_indx]

    window_num = math.floor((stream_end_time - stream_start_time)/window_size) 

    window_start_time = stream_start_time
    burst_size_per_window = []
    burst_time_per_window = []
    for i in range(window_num):
      window_end_time = window_start_time + window_size
      traffic_in_window = self.burst_in_window_size(df, window_start_time, 
                                                    window_end_time, pkt_size_column_indx, pkt_time_column_indx)
      window_time = (window_start_time + window_end_time)/2
      burst_size_per_window.append(traffic_in_window)
      burst_time_per_window.append(window_time)
      window_start_time = window_end_time

    data = {'window time': burst_time_per_window, 'window size': burst_size_per_window}
    windowed_df = pd.DataFrame(data=data)
    return windowed_df  
  
  def test_function(self, df, lower_bound, upper_bound):
    burst_size = 0
    pkt_time_column_indx = df.columns.get_loc('pkt_time')
    pkt_size_column_indx = df.columns.get_loc('pkt_size')
    df.iloc[:,pkt_time_column_indx] = df.iloc[:,pkt_time_column_indx] * 1e6
    start_count_flag = False
    for i in range(len(df)):
      ts = df.iloc[i, pkt_time_column_indx] 
 
      if(ts > upper_bound):
        break
      if(ts > lower_bound):
        burst_size = burst_size + df.iloc[i, pkt_size_column_indx]
      
    return burst_size

  def raw_dataframe(self):
    ds_name_dict = self.partition_files_base_filename()
    first_trial = True
    samples = [] 
    labels = []
    i = 0
    for key in ds_name_dict.keys():
      for ds in ds_name_dict[key]:
        df_raw = self.make_dataset_dataframe(ds)
        i += 1
        df_raw = df_raw.iloc[:1400,:] 
        label = int(key)
        labels.append(label)
        sample = (df_raw.T).reset_index()
        sample['label'] = label
        # Place to preprocess data
        if first_trial:
          aggregated_df = sample
          first_trial = False
        else:
          aggregated_df = pd.concat([aggregated_df, sample],  ignore_index=True)    
    aggregated_df.drop(columns=['index'], inplace=True)
    return aggregated_df


  def aggregated_dataframe(self, window_size):
    ds_name_dict = self.partition_files_base_filename()
    first_trial = True
    samples = [] 
    labels = []
    items_num = sum(len(value) for value in ds_name_dict.values())
    progress_counter = 0
    # for key in ds_name_dict.keys():
    #   for ds in ds_name_dict[key]:
    #     df_raw = self.make_dataset_dataframe(ds)
    #     #df_raw_copy = df_raw.copy()
    #     label = int(key)
    #     windowed_df = self.burst_in_window_pattern(df_raw, window_size).drop(columns=['window time'])
    #     labels.append(label)
    #     sample = (windowed_df.T).reset_index()
    #     sample['label'] = label
    #     # Place to preprocess data
    #     if first_trial:
    #       aggregated_df = sample
    #       first_trial = False
    #     else:
    #       aggregated_df = pd.concat([aggregated_df, sample], ignore_index=True)
    #     progress_counter += 1
        
    ds_keys = list(ds_name_dict.keys())
    with tqdm(total=len(ds_keys)*len(ds_name_dict[ds_keys[0]]), desc="Processing DataL=:") as pbar:
      for key in ds_keys:
        for ds in ds_name_dict[key]:
          df_raw = self.make_dataset_dataframe(ds)
          #df_raw_copy = df_raw.copy()
          label = int(key)
          windowed_df = self.burst_in_window_pattern(df_raw, window_size).drop(columns=['window time'])
          labels.append(label)
          sample = (windowed_df.T).reset_index()
          sample['label'] = label
          # Place to preprocess data
          if first_trial:
              aggregated_df = sample
              first_trial = False
          else:
              aggregated_df = pd.concat([aggregated_df, sample], ignore_index=True)
          progress_counter += 1
          pbar.update(1)
    aggregated_df.drop(columns=['index'], inplace=True)
    aggregated_df = aggregated_df.fillna(0)
    ## Move label column to the last column
    column_to_move = aggregated_df.pop("label")
    aggregated_df.insert(len(aggregated_df.columns), "label", column_to_move)
    return aggregated_df
