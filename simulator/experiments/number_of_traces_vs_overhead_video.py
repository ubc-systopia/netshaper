import numpy as np
import pandas as pd
from tqdm import tqdm
import matplotlib.pyplot as plt
import configlib

from src.transport import DP_transport, NonDP_transport
from src.transport import DP_transport, NonDP_transport
from src.utils.overhead_utils import norm_overhead, wasserstein_overhead, get_fpa_failure_rate
from src.utils.DP_utils import calculate_privacy_loss, get_noise_multiplier


def get_parallel_dataframe(df, video_num, max_video_per_experiment):
    df = df.drop(columns=['label'])
    df = df.sample(frac=1).reset_index(drop=True)
    # Create the dataframe of parallel traces
    parallel_traces_list = []
    for i in range(0, df.shape[0], video_num):
        if (i + video_num > df.shape[0]):
            continue
        parallel_traces_list.append(df[i:i+video_num].sum(axis=0))
    parallel_df = pd.concat(parallel_traces_list, axis=1).transpose()
    # Add the label column as numbers between 0 and parallel_df.shape[0]
    parallel_df['label'] = np.arange(parallel_df.shape[0]) 
    parallel_df.sample(n=max_video_per_experiment, random_state=1)
    return parallel_df



def number_of_traces_vs_overhead_video(config: configlib.Config, filtered_data):
    ## Initializing parameters
    if(config.middlebox_period_us % config.app_time_resolution_us != 0):
        print("The middlebox period must be a multiple of the application time resolution.")
        return -1
    else:
         DP_step = int(config.middlebox_period_us / config.app_time_resolution_us) 
    
    privacy_window_size_us = config.privacy_window_size_s * 1e6
    num_of_queries = int(privacy_window_size_us / config.middlebox_period_us)
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))
    
    video_nums_list = np.linspace(config.min_video_num, config.max_video_num, config.num_of_video_nums, dtype=int)
    
    baseline_results = {'average_aggregated_overhead_constant_rate_dynamic':[],
                        'std_aggregated_overhead_constant_rate_dynamic': [],
                        'average_aggregated_overhead_constant_rate_anonymized':[],
                        'std_aggregated_overhead_constant_rate_anonymized':[],
                        'std_aggregated_overhead_dynamic_shaping': [],
                        'average_aggregated_overhead_pacer':[],
                        'std_aggregated_overhead_pacer': [],
                        'video_num': []} 
    

    filtered_data_pruned = filtered_data.drop(columns=['video_name'])
    original_data, shaped_data_constant_rate = NonDP_transport(filtered_data_pruned, config.app_time_resolution_us, config.data_time_resolution_us, method="constant-rate") 
    constant_rate_max = shaped_data_constant_rate.max().max()
    print("max burst size: ", constant_rate_max)
    average_aggregated_overhead_constant_rate_dynamic, std_aggregated_overhead_constant_rate_dynamic = norm_overhead(original_data, shaped_data_constant_rate)

    average_aggregated_overhead_constant_rate_anonymized = average_aggregated_overhead_constant_rate_dynamic * config.max_video_num 
    std_aggregated_overhead_constant_rate_anonymized = std_aggregated_overhead_constant_rate_dynamic * config.max_video_num 
    original_data, shaped_data_pacer = NonDP_transport(filtered_data, config.app_time_resolution_us, config.data_time_resolution_us, method="pacer_video")

    average_aggregated_overhead_pacer, std_aggregated_overhead_pacer = norm_overhead(original_data, shaped_data_pacer)
     
    
     
    results = {'aggregated_privacy_loss': [],
               'per_query_privacy_loss': [],
               'noise_multiplier': [],
               'average_aggregated_overhead': [],
               'std_aggregated_overhead': [],
               'alpha': [],
               'video_num': [],
               'dp_interval_us': []}
    
    privacy_losses = config.privacy_losses

    noise_multipliers = [get_noise_multiplier(privacy_loss, num_of_queries, alphas, config.delta) for privacy_loss in privacy_losses] 
    with tqdm(total=len(video_nums_list)*len(noise_multipliers)) as pbar:
        for noise_multiplier in noise_multipliers:
            for video_num in video_nums_list:
                max_dp_decision = video_num * constant_rate_max
                print("max_dp_decision: ", max_dp_decision)
                filtered_data_pruned_parallelized = get_parallel_dataframe(filtered_data_pruned, video_num, config.max_videos_per_single_experiment)
                original_data, DP_data, dummy_data = DP_transport(filtered_data_pruned_parallelized, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.sensitivity, DP_step, config.data_time_resolution_us, noise_multiplier=noise_multiplier, min_DP_size=config.min_dp_decision, max_DP_size=max_dp_decision) 

                # Calculate the privacy loss
                aggregated_eps, best_alpha = calculate_privacy_loss(num_of_queries, alphas, noise_multiplier, config.delta) 
                per_query_eps, best_alpha = calculate_privacy_loss(1, alphas, noise_multiplier, config.delta)
                
                # Calculate the overhead of DP transport
                average_aggregated_overhead, std_aggregated_overhead = norm_overhead(original_data, DP_data)
                
                results['aggregated_privacy_loss'].append(aggregated_eps)
                results['per_query_privacy_loss'].append(per_query_eps)
                results['noise_multiplier'].append(noise_multiplier)
                results['average_aggregated_overhead'].append(average_aggregated_overhead)
                results['std_aggregated_overhead'].append(std_aggregated_overhead)
                results['alpha'].append(best_alpha)
                results['video_num'].append(video_num)
                results['dp_interval_us'].append(config.data_time_resolution_us) 

                baseline_results['average_aggregated_overhead_constant_rate_dynamic'].append(average_aggregated_overhead_constant_rate_dynamic)
                baseline_results['std_aggregated_overhead_constant_rate_dynamic'].append(std_aggregated_overhead_constant_rate_dynamic)
                baseline_results['average_aggregated_overhead_constant_rate_anonymized'].append(average_aggregated_overhead_constant_rate_anonymized/ video_num)
                baseline_results['std_aggregated_overhead_constant_rate_anonymized'].append(std_aggregated_overhead_constant_rate_anonymized/video_num)
                baseline_results['average_aggregated_overhead_pacer'].append(average_aggregated_overhead_pacer)
                baseline_results['std_aggregated_overhead_pacer'].append(std_aggregated_overhead_pacer)
                baseline_results['video_num'].append(video_num)
                
                
                pbar.update(1) 
        
    return baseline_results, results 