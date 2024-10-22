import pandas as pd
import numpy as np  
from scipy.stats import wasserstein_distance



def get_pacer_web_overhead(original_df):
    # Dropping label column
    original_df = original_df.drop(columns=['label'])

    # Getting trace sizes
    trace_sizes = original_df.sum(axis=1).to_numpy()
    
    # Getting the maximum trace sizes
    max_trace_size = np.max(trace_sizes)

    # The overhead of each trace if it was reshaped to maximum trace size
    overheads = max_trace_size - trace_sizes
    

    overhead_df = pd.DataFrame({'sum_diff': overheads, 'sum_original': trace_sizes},)
   
    return overhead_df
       

def get_fpa_failure_rate(original_df, fpa_df):
    # Iterating over the rows of the original df
    diff   = fpa_df.sum(axis=1) - original_df.sum(axis=1) 
    # calculate number of negative values in the diff
    negative_diff = diff[diff < 0]
    return len(negative_diff)/len(diff)


def df_zero_padding_row(original_df, remapped_df):
  assert original_df.shape[0]==remapped_df.shape[0], "The number of rows should be the same!"
  # We assume original df always has smaller number of columns and should be padded  
  num_of_cols = remapped_df.shape[1] -  original_df.shape[1]
  assert num_of_cols >= 0, "The original df is larger, something is wrong with the input!"
  padding_array = np.zeros((original_df.shape[0], num_of_cols))
  padding_df = pd.DataFrame(padding_array.T)
  padded_df = pd.concat([original_df.T, padding_df]).reset_index()
  padded_df = padded_df.drop(columns=['index']).T
  return padded_df
 


def overhead_aggregation(overhead_df_server_to_client, overhead_df_client_to_server):
    if overhead_df_client_to_server is None:
        total_overhead = overhead_df_server_to_client
    else:
        total_overhead = overhead_df_client_to_server + overhead_df_server_to_client

    relative_overhead = total_overhead['sum_diff'] / total_overhead['sum_original']
    return relative_overhead.mean(), relative_overhead.std()

def overhead(original_df, remapped_df): 
    if (original_df.shape != remapped_df.shape):
        original_df = df_zero_padding_row(original_df, remapped_df)   

    diff_df = remapped_df - original_df
    # create a data frame with first column as the sum of the differences and the second column as the sum of the original df
    overhead_df = pd.DataFrame({'sum_diff': diff_df.sum(axis=1), 'sum_original': original_df.sum(axis=1)}) 
    return overhead_df


def norm_overhead(original_df, remapped_df):
  if (original_df.shape != remapped_df.shape):
    original_df = df_zero_padding_row(original_df, remapped_df) 
    
  diff_df = remapped_df - original_df
  diff_df['sum_diff'] = diff_df.sum(axis=1)/original_df.sum(axis=1)
#   diff_df['norm_diff'] = np.sqrt(np.square(diff_df.loc[:, diff_df.columns!='sum_diff']).sum(axis=1)) / np.sqrt(np.square(original_df.loc[:, original_df.columns!='sum_diff']).sum(axis=1))
  
  average_sum_diff = diff_df['sum_diff'].mean()
  std_sum_diff = diff_df['sum_diff'].std()
#   average_norm_diff = diff_df['norm_diff'].mean()

#   min_sum_diff = diff_df['sum_diff'].min()
#   min_norm_diff = diff_df['norm_diff'].min() 

#   max_sum_diff = diff_df['sum_diff'].max()
#   max_norm_diff = diff_df['norm_diff'].max()

  return average_sum_diff, std_sum_diff

def normalization_l1(df):
  norm_1 = abs(df).sum(axis=1) + 1e-10
  normalized_df = df.T/norm_1
  return normalized_df.T

def wasserstein_overhead(original_df, remapped_df):
  if (original_df.shape != remapped_df.shape):
      original_df = df_zero_padding_row(original_df, remapped_df) 
  # Make streams distribution
  original_df_distribution = normalization_l1(original_df)
  remapped_df_distribution = normalization_l1(remapped_df)
  #display(original_df_distribution.sum(axis=1)) 
  burst_numbers = range(len(original_df.columns))
  dists = []
  for i in range(len(original_df)):
    original_stream = list(original_df_distribution.iloc[i,:])
    remapped_stream = list(remapped_df_distribution.iloc[i,:])
    dist = wasserstein_distance(burst_numbers, burst_numbers, original_stream, remapped_stream)
    dists.append(dist)
  return sum(dists)/len(dists)