import numpy as np
from tqdm import tqdm

import configlib


from src.transport import DP_transport, NonDP_transport
from src.utils.overhead_utils import norm_overhead, wasserstein_overhead
from src.utils.DP_utils import calculate_privacy_loss, get_noise_multiplier

def privacy_loss_vs_overhead(config: configlib.Config, filtered_data):
    
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
    privacy_losses = np.linspace(config.privacy_loss_min, config.privacy_loss_max, config.privacy_loss_num)
    
    # Baseline overheads 
    original_data, shaped_data = NonDP_transport(filtered_data, config.app_time_resolution_us, config.data_time_resolution_us) 

    baseline_results = {'average_aggregated_overhead': [], 'average_norm_distance': [], 'average_wasserstein_distance': []} 
 
    _, average_aggregated_overhead, _, _, average_norm_distance, _ = norm_overhead(original_data, shaped_data)
    
    baseline_results['average_aggregated_overhead'].append(average_aggregated_overhead)
    baseline_results['average_norm_distance'].append(average_norm_distance)
    baseline_results['average_wasserstein_distance'].append(wasserstein_overhead(original_data, shaped_data))
    
    # Shaping Overheads  
    results = {'privacy_loss': [], 'noise_multiplier': [],  'average_aggregated_overhead': [], 'average_norm_distance': [], 'average_wasserstein_distance': [], 'alpha': []} 
              
    with tqdm(total=config.privacy_loss_num ) as pbar:
        for privacy_loss in privacy_losses:
            # Calculate the noise multiplier based on the privacy loss
            noise_multiplier = get_noise_multiplier(privacy_loss, num_of_queries, alphas, config.delta)
            
            # Applying differentially private transport of data
            original_data, DP_data, dummy_data = DP_transport(filtered_data, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.sensitivity, DP_step, config.data_time_resolution_us, noise_multiplier=noise_multiplier) 
            
    
            
            # Calculating the total privacy loss
            eps, best_alpha = calculate_privacy_loss(num_of_queries, alphas, noise_multiplier, config.delta)
            # print(eps)
            # print(norm_overhead(original_data, DP_data)) 
            # print("acc: " + str(acc)) 
            # Saving the results
            results['noise_multiplier'].append(noise_multiplier)
            results['privacy_loss'].append(eps)
            results['alpha'].append(best_alpha)
            
            # Calculating the overhead of the DP transport
            _, average_aggregated_overhead, _, _, average_norm_distance, _ = norm_overhead(original_data, DP_data)
            
            wasserstein_dist = wasserstein_overhead(original_data, DP_data)
            
            results['average_aggregated_overhead'].append(average_aggregated_overhead)
            results['average_norm_distance'].append(average_norm_distance)
            results['average_wasserstein_distance'].append(wasserstein_dist)
            pbar.update(1)
    
    return baseline_results, results
    