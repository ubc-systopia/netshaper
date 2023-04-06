from tqdm import tqdm
import configlib

from src.utils.DP_utils import calculate_privacy_loss
from src.utils.overhead_utils import norm_overhead, wasserstein_overhead


def overhead_analysis(config: configlib.Config, data_list):
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))   

    results = {'noise_multiplier': [], 'privacy_loss': [], 'aggregated_overhead': [], 'norm_overhead': [], 'wasserstein_distance': [], 'DP_interval_length_us': [], 'send_interval_length_us': []}
    with tqdm(total=len(data_list)) as pbar:
        for data in data_list:
            # print(data['noise_multiplier'])
            results['noise_multiplier'].append(data['noise_multiplier']) 

            steps = int(config.privacy_window_length_s * 1e6 / data['DP_interval_length_us']) 
            results['privacy_loss'].append(calculate_privacy_loss(steps, alphas, data['noise_multiplier'], config.delta))

            _, average_aggregated_overhead, _, _, average_norm_distance, _ = norm_overhead(data['unshaped_data'], data['shaped_data'])
                
            wasserstein_dist = wasserstein_overhead(data['unshaped_data'], data['shaped_data'])

            results['aggregated_overhead'].append(average_aggregated_overhead)
            results['norm_overhead'].append(average_norm_distance)
            results['wasserstein_distance'].append(wasserstein_dist)
            
            results['DP_interval_length_us'].append(data['DP_interval_length_us'])
            results['send_interval_length_us'].append(data['send_interval_length_us']) 
            pbar.update(1)
        
    return results 