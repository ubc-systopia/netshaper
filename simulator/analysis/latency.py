from tqdm import tqdm
import configlib
import numpy as np
from src.data_utils.testbed_data_processor import get_aggregated_RTTs
from src.utils.DP_utils import calculate_privacy_loss

def latency_analysis(config: configlib.Config, data_list):
    results = {'peer1_noise_multiplier': [], 'peer2_noise_multiplier': [], 'peer1_DP_interval_length_us': [], 'peer2_DP_interval_length_us': [],
    'peer1_aggregated_privacy_loss': [], 'peer2_aggregated_privacy_loss': [],
    'peer1_per_query_privacy_loss': [], 'peer2_per_query_privacy_loss': [],
    'peer1_send_interval_length_us': [], 'peer2_send_interval_length_us': [],'peer1_max_dp_decision': [], 'peer1_min_dp_decision': [], 'peer2_max_dp_decision': [], 'peer2_min_dp_decision': [], 'latencies_average_us': [], 'latencies_std_us': [], 'latencies_95percentile_us': [],
    'peer1_sensitivity': [], 'peer2_sensitivity': []}
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))   
    print(len(data_list))
    for data in data_list:
        results['peer1_noise_multiplier'].append(data['peer1_noise_multiplier'])
        results['peer2_noise_multiplier'].append(data['peer2_noise_multiplier'])

        results['peer1_DP_interval_length_us'].append(data['peer1_DP_interval_length_us'])
        results['peer2_DP_interval_length_us'].append(data['peer2_DP_interval_length_us'])
        
        results['peer1_max_dp_decision'].append(data['peer1_maxDecisionSize'])  
        results['peer2_max_dp_decision'].append(data['peer2_maxDecisionSize']) 
        
        results['peer1_min_dp_decision'].append(data['peer1_minDecisionSize']) 
        results['peer2_min_dp_decision'].append(data['peer2_minDecisionSize']) 
        
        results['peer1_send_interval_length_us'].append(data['peer1_send_interval_length_us'])
        results['peer2_send_interval_length_us'].append(data['peer2_send_interval_length_us']) 
        
        results['peer1_sensitivity'].append(data['peer1_sensitivity']) 
        results['peer2_sensitivity'].append(data['peer2_sensitivity'])
         
        steps_peer1 = int(config.privacy_window_length_s * 1e6 / data['peer1_DP_interval_length_us']) 
        steps_peer2 = int(config.privacy_window_length_s * 1e6 / data['peer2_DP_interval_length_us']) 

        results['peer1_aggregated_privacy_loss'].append(calculate_privacy_loss(steps_peer1, alphas, data['peer1_noise_multiplier'], config.delta)[0])
        results['peer2_aggregated_privacy_loss'].append(calculate_privacy_loss(steps_peer2, alphas, data['peer2_noise_multiplier'], config.delta)[0])
        
        results['peer1_per_query_privacy_loss'].append(calculate_privacy_loss(1, alphas, data['peer1_noise_multiplier'], config.delta))
        results['peer2_per_query_privacy_loss'].append(calculate_privacy_loss(1, alphas, data['peer2_noise_multiplier'], config.delta))
        
            
        results['latencies_average_us'].append(np.mean(data['RTTs'])/1e3)
        results['latencies_std_us'].append(np.std(data['RTTs'])/1e3)
        results['latencies_95percentile_us'].append(np.percentile(data['RTTs'], 95)/1e3)
    
    return results