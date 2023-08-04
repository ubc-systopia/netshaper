import numpy as np
import pandas as pd
from tqdm import tqdm
import matplotlib.pyplot as plt
import configlib

from src.transport import DP_transport, NonDP_transport
from src.utils.overhead_utils import overhead, overhead_aggregation, wasserstein_overhead, get_fpa_failure_rate, get_pacer_web_overhead
from src.utils.DP_utils import calculate_privacy_loss, get_noise_multiplier


def get_parallel_dataframe(df, webpage_num, max_webpage_per_experiment):
    df = df.drop(columns=['label'])
    df = df.sample(frac=1).reset_index(drop=True)
    # Create the dataframe of parallel traces
    parallel_traces_list = []
    for i in range(0, df.shape[0], webpage_num):
        if (i + webpage_num > df.shape[0]):
            continue
        parallel_traces_list.append(df[i:i+webpage_num].sum(axis=0))
    parallel_df = pd.concat(parallel_traces_list, axis=1).transpose()
    # Add the label column as numbers between 0 and parallel_df.shape[0]
    parallel_df['label'] = np.arange(parallel_df.shape[0]) 
    parallel_df.sample(n=max_webpage_per_experiment, random_state=1)
    return parallel_df


# TODO: needs modification
def number_of_traces_vs_overhead_web_unidirectional(config: configlib.Config, filtered_data):

    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))

    
    # From server to client data and parameters
    filtered_data_s_to_c = filtered_data[0]

    if(config.middlebox_period_server_to_client_us % config.app_time_resolution_server_to_client_us != 0):
        raise ValueError("The middlebox period must be a multiple of the application time resolution.")
    else:
        DP_step_s_to_c = int(config.middlebox_period_server_to_client_us / config.app_time_resolution_us_server_to_client_us) 
    
       
        
    privacy_window_size_s_to_c_us = config.privacy_window_size_server_to_client_s * 1e6
    num_of_queries_s_to_c = int(privacy_window_size_s_to_c_us / config.middlebox_period_s_to_c_us)
    
    # Constant-rate shaping and overheads
    original_data_s_to_c, shaped_data_constant_rate_s_to_c = NonDP_transport(filtered_data_s_to_c, config.app_time_resolution_s_to_c_us, config.data_time_resolution_s_to_c_us, method="constant-rate") 
    constant_rate_max_s_to_c = shaped_data_constant_rate_s_to_c.max().max()
    overhead_constant_rate_dynamic_s_to_c = overhead(original_data_s_to_c, shaped_data_constant_rate_s_to_c) 
    
    # Aggregation of overhead for constant-rate shaping in one direction    
    average_aggregated_overhead_constant_rate_dynamic, std_aggregated_overhead_constant_rate_dynamic = overhead_aggregation(overhead_server_to_client=overhead_constant_rate_dynamic_s_to_c,
     overhead_df_client_to_server=None) 

    average_aggregated_overhead_constant_rate_anonymized = average_aggregated_overhead_constant_rate_dynamic * config.max_web_num
    std_aggregated_overhead_constant_rate_anonymized = std_aggregated_overhead_constant_rate_dynamic * config.max_web_num


    # Pacer shaping and overheads
    overhead_pacer_s_to_c = get_pacer_web_overhead(filtered_data_s_to_c)
    
    # Aggregation of overhead for pacer shaping in one direction
    average_aggregated_overhead_pacer, std_aggregated_overhead_pacer = overhead_aggregation(overhead_pacer_s_to_c)
    


    noise_multipliers_s_to_c = [get_noise_multiplier(privacy_loss, num_of_queries_s_to_c, alphas, config.delta_s_to_c) for privacy_loss in config.privacy_losses_s_to_c] 
    
    
    baseline_results = {'average_aggregated_overhead_constant_rate_dynamic':[],
                        'std_aggregated_overhead_constant_rate_dynamic': [],
                        'average_aggregated_overhead_constant_rate_anonymized':[],
                        'std_aggregated_overhead_constant_rate_anonymized':[],
                        'std_aggregated_overhead_dynamic_shaping': [],
                        'average_aggregated_overhead_pacer':[],
                        'std_aggregated_overhead_pacer': [],
                        'webpage_num': []}    
     
    results = {'aggregated_privacy_loss_client_to_server': None,
               'aggregated_privacy_loss_server_to_client': [],
               'per_query_privacy_loss_client_to_server': None,
               'per_query_privacy_loss_server_to_client': [],
               'noise_multiplier_server_to_client': [],
               'noise_multiplier_client_to_server': None,
               'average_aggregated_overhead': [],
               'std_aggregated_overhead': [],
               'alpha_server_to_client': [],
               'alpha_client_to_server': None,
               'webpage_num': [],
               'dp_interval_server_to_client_us': [],
               'dp_interval_client_to_server_us': None}
    
    webpage_nums_list = np.linspace(config.min_webpage_num, config.max_webpage_num, config.num_of_webpage_nums, dtype=int)

    with tqdm(total=len(webpage_nums_list)*len(noise_multipliers_s_to_c)) as pbar:
        for nm_s_to_c in noise_multipliers_s_to_c:
            for webpage_num in webpage_nums_list:
                max_dp_decision_s_to_c = webpage_num * constant_rate_max_s_to_c

                
                filtered_data_parallelized_s_to_c = get_parallel_dataframe(filtered_data_s_to_c, webpage_num, config.max_videos_per_single_experiment)

                
                original_data_s_to_c, DP_data_s_to_c, dummy_data_s_to_c = DP_transport(filtered_data_parallelized_s_to_c, config.app_time_resolution_server_to_client_us, config.transport_type, config.DP_mechanism, config.sensitivity_server_to_client, DP_step_s_to_c, config.data_time_resolution_server_to_client_us, noise_multiplier=nm_s_to_c, min_DP_size=config.min_dp_decision_server_to_client, max_DP_size=max_dp_decision_s_to_c)
                


                # Calculating the total privacy
                aggregated_eps_s_to_c, best_alpha_s_to_c = calculate_privacy_loss(num_of_queries_s_to_c, alphas, nm_s_to_c, config.delta_server_to_client) 

                
                # Calculating the per query privacy loss
                per_query_eps_s_to_c, best_alpha_s_to_c = calculate_privacy_loss(1, alphas, nm_s_to_c, config.delta_server_to_client)
                
                                
                # Calculate the overhead
                overhead_s_to_c = overhead(original_data_s_to_c, DP_data_s_to_c) 
                   
                
                total_overhead_mean, total_overhead_std = overhead_aggregation(overhead_s_to_c, None)
                

                # Shaping results
                results['aggregated_privacy_loss_server_to_client'].append(aggregated_eps_s_to_c)
                results['per_query_privacy_loss_server_to_client'].append(per_query_eps_s_to_c)
                results['noise_multiplier_server_to_client'].append(nm_s_to_c)
                results['average_aggregated_overhead'].append(total_overhead_mean)
                results['std_aggregated_overhead'].append(total_overhead_std)
                results['alpha_server_to_client'].append(best_alpha_s_to_c)
                results['web_num'].append(webpage_num)
                results['dp_interval_server_to_client_us'].append(config.middlebox_period_server_to_client_us)
                
               
                # Baseline results
                baseline_results['average_aggregated_overhead_constant_rate_dynamic'].append(average_aggregated_overhead_constant_rate_dynamic)
                baseline_results['std_aggregated_overhead_constant_rate_dynamic'].append(std_aggregated_overhead_constant_rate_dynamic)
                baseline_results['average_aggregated_overhead_constant_rate_anonymized'].append(average_aggregated_overhead_constant_rate_anonymized/webpage_num)
                baseline_results['std_aggregated_overhead_constant_rate_anonymized'].append(std_aggregated_overhead_constant_rate_anonymized/webpage_num)
                # TODO: needs modification
                baseline_results['average_aggregated_overhead_pacer'].append(average_aggregated_overhead_pacer)
                baseline_results['std_aggregated_overhead_pacer'].append(std_aggregated_overhead_pacer)

                pbar.update(1)

    return results, baseline_results



def number_of_traces_vs_overhead_web_bidirectional(config: configlib.Config, filtered_data):
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))

    
    # From server to client data and parameters
    filtered_data_s_to_c = filtered_data[0]

    if(config.middlebox_period_server_to_client_us % config.app_time_resolution_server_to_client_us != 0):
        raise ValueError("The middlebox period must be a multiple of the application time resolution.")
    else:
        DP_step_s_to_c = int(config.middlebox_period_server_to_client_us / config.app_time_resolution_us_server_to_client_us) 
    
       
        
    privacy_window_size_s_to_c_us = config.privacy_window_size_server_to_client_s * 1e6
    num_of_queries_s_to_c = int(privacy_window_size_s_to_c_us / config.middlebox_period_s_to_c_us)
    
    # Baseline from server to client  
    original_data_s_to_c, shaped_data_constant_rate_s_to_c = NonDP_transport(filtered_data_s_to_c, config.app_time_resolution_s_to_c_us, config.data_time_resolution_s_to_c_us, method="constant-rate") 
    constant_rate_max_s_to_c = shaped_data_constant_rate_s_to_c.max().max()
    overhead_constant_rate_dynamic_s_to_c = overhead(original_data_s_to_c, shaped_data_constant_rate_s_to_c) 
    
    
    
    # Pacer from server to client
    overhead_pacer_s_to_c = get_pacer_web_overhead(filtered_data_s_to_c) 
    

     
    noise_multipliers_s_to_c = [get_noise_multiplier(privacy_loss, num_of_queries_s_to_c, alphas, config.delta_s_to_c) for privacy_loss in config.privacy_losses_s_to_c]  


    # From client to server data and parameters (if bidirectional) 
    filtered_data_c_to_s = filtered_data[1]

    if(config.middlebox_period_c_to_s_us % config.app_time_resolution_c_to_s_us != 0): 
        raise ValueError("The middlebox period must be a multiple of the application time resolution.")
    else: 
        DP_step_c_to_s = int(config.middlebox_period_client_to_server_us / config.app_time_resolution_client_to_server_us)
        
    
    privacy_window_size_c_to_s_us = config.privacy_window_size_c_to_s_s * 1e6
    num_of_queries_c_to_s = int(privacy_window_size_c_to_s_us / config.middlebox_period_c_to_s_us)
    
    # Baseline from client to server        
    original_data_c_to_s, shaped_data_constant_rate_c_to_s = NonDP_transport(filtered_data_c_to_s, config.app_time_resolution_c_to_s, config.data_time_resolution_c_to_s_us, method="constant-rate") 
    constant_rate_max_c_to_s = shaped_data_constant_rate_c_to_s.max().max()
    overhead_constant_rate_dynamic_c_to_s = overhead(original_data_c_to_s, shaped_data_constant_rate_c_to_s)  


    # Pacer from client to server
    overhead_pacer_c_to_s = get_pacer_web_overhead(filtered_data_c_to_s) 
    
    # aggregation of overhead for baseline in both directions
    average_aggregated_overhead_constant_rate_dynamic, std_aggregated_overhead_constant_rate_dynamic = overhead_aggregation(overhead_df_server_to_client=overhead_constant_rate_dynamic_s_to_c,
    overhead_df_client_to_server=overhead_constant_rate_dynamic_c_to_s) 
    
    average_aggregated_overhead_constant_rate_anonymized = average_aggregated_overhead_constant_rate_dynamic * config.max_web_num

    std_aggregated_overhead_constant_rate_anonymized = std_aggregated_overhead_constant_rate_dynamic * config.max_web_num


    # aggregation of overhead for pacer in both directions
    average_aggregated_overhead_pacer, std_aggregated_overhead_pacer = overhead_aggregation(overhead_df_server_to_client=overhead_pacer_s_to_c,
                         overhead_df_client_to_server=overhead_pacer_c_to_s)  
    
    

    noise_multipliers_c_to_s = [get_noise_multiplier(privacy_loss, num_of_queries_c_to_s, alphas, config.delta_c_to_s) for privacy_loss in config.privacy_losses_c_to_s]  


    baseline_results = {'average_aggregated_overhead_constant_rate_dynamic':[],
                        'std_aggregated_overhead_constant_rate_dynamic': [],
                        'average_aggregated_overhead_constant_rate_anonymized':[],
                        'std_aggregated_overhead_constant_rate_anonymized':[],
                        'std_aggregated_overhead_dynamic_shaping': [],
                        'average_aggregated_overhead_pacer':[],
                        'std_aggregated_overhead_pacer': [],
                        'webpage_num': []}    
     
    results = {'aggregated_privacy_loss_client_to_server': [],
               'aggregated_privacy_loss_server_to_client': [],
               'per_query_privacy_loss_client_to_server': [],
               'per_query_privacy_loss_server_to_client': [],
               'noise_multiplier_server_to_client': [],
               'noise_multiplier_client_to_server': [],
               'average_aggregated_overhead': [],
               'std_aggregated_overhead': [],
               'alpha_server_to_client': [],
               'alpha_client_to_server': [],
               'webpage_num': [],
               'dp_interval_server_to_client_us': [],
               'dp_interval_client_to_server_us': []}
    
    webpage_nums_list = np.linspace(config.min_webpage_num, config.max_webpage_num, config.num_of_webpage_nums, dtype=int)

    with tqdm(total=len(webpage_nums_list)*len(noise_multipliers_s_to_c)) as pbar:
        for nm_s_to_c, nm_c_to_s in zip(noise_multipliers_s_to_c, noise_multipliers_c_to_s):
            for webpage_num in webpage_nums_list:

                max_dp_decision_s_to_c = webpage_num * constant_rate_max_s_to_c

                max_dp_decision_c_to_s = webpage_num * constant_rate_max_c_to_s

                
                filtered_data_parallelized_s_to_c = get_parallel_dataframe(filtered_data_s_to_c, webpage_num, config.max_videos_per_single_experiment)

                filtered_data_parallelized_c_to_s = get_parallel_dataframe(filtered_data_c_to_s, webpage_num, config.max_videos_per_single_experiment)
                
                
                original_data_s_to_c, DP_data_s_to_c, dummy_data_s_to_c = DP_transport(filtered_data_parallelized_s_to_c, config.app_time_resolution_server_to_client_us, config.transport_type, config.DP_mechanism, config.sensitivity_server_to_client, DP_step_s_to_c, config.data_time_resolution_server_to_client_us, noise_multiplier=nm_s_to_c, min_DP_size=config.min_dp_decision_server_to_client, max_DP_size=max_dp_decision_s_to_c)
                
                original_data_c_to_s, DP_data_c_to_s, dummy_data_c_to_s = DP_transport(filtered_data_parallelized_c_to_s, config.app_time_resolution_client_to_server_us, config.transport_type, config.DP_mechanism, config.sensitivity, DP_step_c_to_s, config.data_time_resolution_client_to_server_us, noise_multiplier=nm_c_to_s, min_DP_size=config.min_dp_decision_client_to_server, max_DP_size=max_dp_decision_c_to_s)


                # Calculating the total privacy
                aggregated_eps_s_to_c, best_alpha_s_to_c = calculate_privacy_loss(num_of_queries_s_to_c, alphas, nm_s_to_c, config.delta_server_to_client) 

                aggregated_eps_c_to_s, best_alpha_c_to_s = calculate_privacy_loss(num_of_queries_c_to_s, alphas, nm_c_to_s, config.delta_client_to_server)
                
                
                # Calculating the per query privacy loss
                per_query_eps_s_to_c, best_alpha_s_to_c = calculate_privacy_loss(1, alphas, nm_s_to_c, config.delta_server_to_client)
                
                per_query_eps_c_to_s, best_alpha_c_to_s = calculate_privacy_loss(1, alphas, nm_c_to_s, config.delta_client_to_server) 
                 
                                
                # Calculate the overhead
                overhead_s_to_c = overhead(original_data_s_to_c, DP_data_s_to_c) 
                   
                overhead_c_to_s = overhead(original_data_c_to_s, DP_data_c_to_s)
                
                total_overhead_mean, total_overhead_std = overhead_aggregation(overhead_s_to_c, overhead_c_to_s)
                

                # Shaping results
                results['aggregated_privacy_loss_client_to_server'].append(aggregated_eps_c_to_s)
                results['aggregated_privacy_loss_server_to_client'].append(aggregated_eps_s_to_c)
                results['per_query_privacy_loss_client_to_server'].append(per_query_eps_c_to_s)
                results['per_query_privacy_loss_server_to_client'].append(per_query_eps_s_to_c)
                results['noise_multiplier_client_to_server'].append(nm_c_to_s) 
                results['noise_multiplier_server_to_client'].append(nm_s_to_c)
                results['average_aggregated_overhead'].append(total_overhead_mean)
                results['std_aggregated_overhead'].append(total_overhead_std)
                results['alpha_server_to_client'].append(best_alpha_s_to_c)
                results['alpha_client_to_server'].append(best_alpha_c_to_s)
                results['web_num'].append(webpage_num)
                results['dp_interval_client_to_server_us'].append(config.middlebox_period_client_to_server_us)
                results['dp_interval_server_to_client_us'].append(config.middlebox_period_server_to_client_us)
                
               
                # Baseline results
                baseline_results['average_aggregated_overhead_constant_rate_dynamic'].append(average_aggregated_overhead_constant_rate_dynamic)
                baseline_results['std_aggregated_overhead_constant_rate_dynamic'].append(std_aggregated_overhead_constant_rate_dynamic)
                baseline_results['average_aggregated_overhead_constant_rate_anonymized'].append(average_aggregated_overhead_constant_rate_anonymized/webpage_num)
                baseline_results['std_aggregated_overhead_constant_rate_anonymized'].append(std_aggregated_overhead_constant_rate_anonymized/webpage_num)
                # TODO: needs modification
                baseline_results['average_aggregated_overhead_pacer'].append(average_aggregated_overhead_pacer)
                baseline_results['std_aggregated_overhead_pacer'].append(std_aggregated_overhead_pacer)
                
                
                
                pbar.update(1)
    return results, baseline_results 
                
                
                
                

def number_of_traces_vs_overhead_web(config: configlib.Config, filtered_data):
    if config.communication_type == "bidirectional":
        return number_of_traces_vs_overhead_web_bidirectional(config, filtered_data)
    elif config.communication_type == "unidirectional":
        return number_of_traces_vs_overhead_web_unidirectional(config, filtered_data)
    else: 
        raise ValueError("The communication type must be either bidirectional or unidirectional.")