from tqdm import tqdm
import configlib
import numpy as np
from src.utils.DP_utils import calculate_privacy_loss
from src.utils.overhead_utils import norm_overhead, wasserstein_overhead




def overhead_analysis(config: configlib.Config, data_list):
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))   

    results = {'peer1_noise_multiplier': [], 'peer2_noise_multiplier': [], 'peer1_aggregated_privacy_loss': [], 'peer2_aggregated_privacy_loss': [],'peer1_per_query_privacy_loss': [], 'peer2_per_query_privacy_loss': [],'aggregated_overhead': [], 'norm_overhead': [], 'wasserstein_distance': [], 'peer1_DP_interval_length_us': [], 'peer1_send_interval_length_us': [], 'peer2_DP_interval_length_us': [], 'peer2_send_interval_length_us': []}
    with tqdm(total=len(data_list)) as pbar:
        for data in data_list:
            # print(data['noise_multiplier'])
            print(data.keys())
            results['peer1_noise_multiplier'].append(data['peer1_noise_multiplier']) 
            results['peer2_noise_multiplier'].append(data['peer2_noise_multiplier']) 

            

            steps_peer1 = int(config.privacy_window_length_s * 1e6 / data['peer1_DP_interval_length_us']) 
            steps_peer2 = int(config.privacy_window_length_s * 1e6 / data['peer2_DP_interval_length_us']) 

            results['peer1_aggregated_privacy_loss'].append(calculate_privacy_loss(steps_peer1, alphas, data['peer1_noise_multiplier'], config.delta)[0])
            results['peer2_aggregated_privacy_loss'].append(calculate_privacy_loss(steps_peer2, alphas, data['peer2_noise_multiplier'], config.delta)[0])

            results['peer1_per_query_privacy_loss'].append(calculate_privacy_loss(1, alphas, data['peer1_noise_multiplier'], config.delta)[0])
            results['peer2_per_query_privacy_loss'].append(calculate_privacy_loss(1, alphas, data['peer2_noise_multiplier'], config.delta)[0])
            
             
            _, average_aggregated_overhead, _, _, average_norm_distance, _ = norm_overhead(data['unshaped_data'], data['shaped_data'])
            # print("overhead by old function: ", average_aggregated_overhead) 
            # print("overhead by new function: ", test_overhead_function(data['unshaped_data'], data['shaped_data']))


            # original_data_arr = data['unshaped_data'].drop(columns=['label']).to_numpy()
            # original_data_arr_reshaped = np.reshape(original_data_arr, (-1)) 
            # DP_data_arr = data['shaped_data'].drop(columns=['label']).to_numpy()
            # DP_data_arr_reshaped = np.reshape(DP_data_arr, (-1))
            # print("noise_multiplier: ", data['peer2_noise_multiplier'])
            # print("DP_decision_sum: ", "{:e}".format(np.sum(DP_data_arr_reshaped))) 
            # print("data_sum: ", "{:e}".format(np.sum(original_data_arr_reshaped)))  
                  
            wasserstein_dist = wasserstein_overhead(data['unshaped_data'], data['shaped_data'])

            results['aggregated_overhead'].append(average_aggregated_overhead)
            results['norm_overhead'].append(average_norm_distance)
            results['wasserstein_distance'].append(wasserstein_dist)
            
            results['peer1_DP_interval_length_us'].append(data['peer1_DP_interval_length_us'])
            results['peer1_send_interval_length_us'].append(data['peer1_send_interval_length_us']) 

            results['peer2_DP_interval_length_us'].append(data['peer2_DP_interval_length_us'])
            results['peer2_send_interval_length_us'].append(data['peer2_send_interval_length_us']) 


            pbar.update(1)
        
    return results 