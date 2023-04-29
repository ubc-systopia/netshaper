import numpy as np
from tqdm import tqdm
import matplotlib.pyplot as plt
import configlib


from src.transport import DP_transport, NonDP_transport
from src.utils.overhead_utils import norm_overhead, wasserstein_overhead, get_fpa_failure_rate
from src.utils.DP_utils import calculate_privacy_loss, get_noise_multiplier


def noise_multiplier_vs_overhead(config: configlib.Config, filtered_data):
    
    ## Initializing parameters
    if(config.middlebox_period_us % config.app_time_resolution_us != 0):
        print("The middlebox period must be a multiple of the application time resolution.")
        return -1
    else:
         DP_step = int(config.middlebox_period_us / config.app_time_resolution_us) 
    
    privacy_window_size_us = config.privacy_window_size_s * 1e6
    num_of_queries = int(privacy_window_size_us / config.middlebox_period_us)
    ## privacy loss parameters
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))   
    noise_multipliers = config.noise_multipliers    

    # Baseline overheads 
    baseline_results = {'average_aggregated_overhead_constant_rate': [],
                        'average_norm_distance_constant_rate': [], 'average_wasserstein_distance_constant_rate': [],
                        'average_aggregated_overhead_pacer': [],
                        'average_norm_distance_pacer': [], 'average_wasserstein_distance_pacer': []}


    
    # Baseline: Constant Rate
    filtered_data_pruned = filtered_data.drop(columns=['video_name'])
    original_data, shaped_data_constant_rate = NonDP_transport(filtered_data_pruned, config.app_time_resolution_us, config.data_time_resolution_us, method="constant-rate") 
    _, average_aggregated_overhead_constant_rate, _, _, average_norm_distance_constant_rate, _ = norm_overhead(original_data, shaped_data_constant_rate)
    baseline_results['average_aggregated_overhead_constant_rate'].append(average_aggregated_overhead_constant_rate)
    baseline_results['average_norm_distance_constant_rate'].append(average_norm_distance_constant_rate)
    baseline_results['average_wasserstein_distance_constant_rate'].append(wasserstein_overhead(original_data, shaped_data_constant_rate))
    
    
    # Baseline: Pacer
    original_data, shaped_data_pacer = NonDP_transport(filtered_data, config.app_time_resolution_us, config.data_time_resolution_us, method="pacer")
    baseline_results['average_aggregated_overhead_pacer'].append(shaped_data_pacer.sum(axis=1).sum()/original_data.sum(axis=1).sum() - 1)
    # baseline_results['average_wasserstein_distance_pacer'].append(wasserstein_overhead(original_data, shaped_data_pacer)) 
    
    # Shaping Overheads  
    results = {'aggregated_privacy_loss': [],
               'per_query_privacy_loss': [],
               "dp_decision_interval_us":[],
               'noise_multiplier': [],
               'average_aggregated_overhead': [],
               'average_norm_distance': [],
               'average_wasserstein_distance': [],
               'alpha': [],
               'average_aggregated_overhead_fpa': [],
               'average_norm_distance_fpa': [],
               'average_wasserstein_distance_fpa': [],
               'fpa_failure_rate': []
               } 
              
    with tqdm(total=len(noise_multipliers) ) as pbar:
        for noise_multiplier in noise_multipliers:
            # Calculate the noise multiplier based on the privacy loss
            original_data, DP_data, dummy_data = DP_transport(filtered_data_pruned, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.sensitivity, DP_step, config.data_time_resolution_us, noise_multiplier=noise_multiplier, min_DP_size=config.min_dp_decision, max_DP_size=config.max_dp_decision) 

            # Calculating the total privacy loss
            aggregated_eps, best_alpha = calculate_privacy_loss(num_of_queries, alphas, noise_multiplier, config.delta)
            per_query_eps, best_alpha = calculate_privacy_loss(1, alphas, noise_multiplier, config.delta)
            # print(eps)
            # print(norm_overhead(original_data, DP_data)) 
            # print("acc: " + str(acc)) 
            # Saving the results
            results['noise_multiplier'].append(noise_multiplier)
            results['aggregated_privacy_loss'].append(aggregated_eps)
            results['per_query_privacy_loss'].append(per_query_eps)
            results['alpha'].append(best_alpha)
            results['dp_decision_interval_us'].append(config.data_time_resolution_us)
            
            # Calculating the overhead of the DP transport
            _, average_aggregated_overhead, _, _, average_norm_distance, _ = norm_overhead(original_data, DP_data)
            
            wasserstein_dist = wasserstein_overhead(original_data, DP_data)
            
            results['average_aggregated_overhead'].append(average_aggregated_overhead)
            results['average_norm_distance'].append(average_norm_distance)
            results['average_wasserstein_distance'].append(wasserstein_dist)
            

            # Calculating the overhead of the FPA transport
            
            original_data, FPA_data, dummy_data = DP_transport(filtered_data_pruned, config.app_time_resolution_us, "DP_static", config.DP_mechanism, config.sensitivity_fpa, DP_step, config.data_time_resolution_us,global_epsilon=aggregated_eps)
            # print("Original data shape: ", original_data.shape)
            # print("FPA data shape: ", FPA_data.head) 
            _, average_aggregated_overhead_fpa, _, _, average_norm_distance_fpa, _ = norm_overhead(original_data, FPA_data)
            
            results['average_aggregated_overhead_fpa'].append(average_aggregated_overhead_fpa)
            results['average_norm_distance_fpa'].append(average_norm_distance_fpa)
            results['average_wasserstein_distance_fpa'].append(wasserstein_overhead(original_data, FPA_data))
            
            results['fpa_failure_rate'].append(get_fpa_failure_rate(original_data, FPA_data))
            

            pbar.update(1)
    
    return baseline_results, results
    