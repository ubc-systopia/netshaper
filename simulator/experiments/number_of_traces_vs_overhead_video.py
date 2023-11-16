import numpy as np
import pandas as pd
from tqdm import tqdm
import matplotlib.pyplot as plt
import configlib

from src.transport import DP_transport, NonDP_transport
from src.utils.overhead_utils import overhead, overhead_aggregation, wasserstein_overhead, get_fpa_failure_rate, get_pacer_web_overhead
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


def number_of_traces_vs_overhead_video_unidirectional(config: configlib.Config, filtered_data):

    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))

    
    # From server to client data and parameters
    filtered_data_s_to_c = filtered_data[0]

    if(config.middlebox_period_server_to_client_us % config.app_time_resolution_server_to_client_us != 0):
        raise ValueError("The middlebox period must be a multiple of the application time resolution.")
    else:
        DP_step_s_to_c = int(config.middlebox_period_server_to_client_us / config.app_time_resolution_server_to_client_us) 
    
       
        
    privacy_window_size_s_to_c_us = config.privacy_window_size_server_to_client_s * 1e6
    num_of_queries_s_to_c = int(privacy_window_size_s_to_c_us / config.middlebox_period_server_to_client_us)
    
    # Constant-rate shaping and overheads
    filtered_data_pruned_s_to_c = filtered_data_s_to_c.drop(columns=['video_name'])
    original_data_s_to_c, shaped_data_constant_rate_s_to_c = NonDP_transport(filtered_data_pruned_s_to_c, config.app_time_resolution_server_to_client_us, config.data_time_resolution_server_to_client_us, method="constant-rate") 
    constant_rate_max_s_to_c = shaped_data_constant_rate_s_to_c.max().max()
    overhead_constant_rate_dynamic_s_to_c = overhead(original_data_s_to_c, shaped_data_constant_rate_s_to_c) 
    
    # Aggregation of overhead for constant-rate shaping in one direction    
    average_aggregated_overhead_constant_rate_dynamic, std_aggregated_overhead_constant_rate_dynamic = overhead_aggregation(overhead_df_server_to_client=overhead_constant_rate_dynamic_s_to_c,
     overhead_df_client_to_server=None) 

    average_aggregated_overhead_constant_rate_anonymized = average_aggregated_overhead_constant_rate_dynamic * config.max_video_num
    std_aggregated_overhead_constant_rate_anonymized = std_aggregated_overhead_constant_rate_dynamic * config.max_video_num


    # Pacer shaping and overheads
    original_data_s_to_c, shaped_data_pacer_s_to_c = NonDP_transport(filtered_data_s_to_c, config.app_time_resolution_server_to_client_us, config.data_time_resolution_server_to_client_us, method="pacer_video")
    overhead_pacer_s_to_c = overhead(original_data_s_to_c, shaped_data_pacer_s_to_c)
    
    # Aggregation of overhead for pacer shaping in one direction
    average_aggregated_overhead_pacer, std_aggregated_overhead_pacer = overhead_aggregation(overhead_df_server_to_client=overhead_pacer_s_to_c,
                         overhead_df_client_to_server=None) 
    


    noise_multipliers_s_to_c = [get_noise_multiplier(privacy_loss, num_of_queries_s_to_c, alphas, config.delta_server_to_client) for privacy_loss in config.privacy_losses_server_to_client] 
    
    
    baseline_results = {'average_aggregated_overhead_constant_rate_dynamic':[],
                        'std_aggregated_overhead_constant_rate_dynamic': [],
                        'average_aggregated_overhead_constant_rate_anonymized':[],
                        'std_aggregated_overhead_constant_rate_anonymized':[],
                        'std_aggregated_overhead_dynamic_shaping': [],
                        'average_aggregated_overhead_pacer':[],
                        'std_aggregated_overhead_pacer': [],
                        'video_num': []}    
     
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
               'video_num': [],
               'dp_interval_server_to_client_us': [],
               'dp_interval_client_to_server_us': None}
    
    video_nums_list = np.linspace(config.min_video_num, config.max_video_num, config.num_of_video_nums, dtype=int)
    # video_nums_list = [1, 16, 128]
    with tqdm(total=len(video_nums_list)*len(noise_multipliers_s_to_c)) as pbar:
        for nm_s_to_c in noise_multipliers_s_to_c:
            for video_num in video_nums_list:
                max_dp_decision_s_to_c = video_num * constant_rate_max_s_to_c
                
                filtered_data_parallelized_s_to_c = get_parallel_dataframe(filtered_data_pruned_s_to_c, video_num, config.max_videos_per_single_experiment)

                
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
                results['video_num'].append(video_num)
                results['dp_interval_server_to_client_us'].append(config.middlebox_period_server_to_client_us)
                              # Printing some results
                print_results = True
                if print_results:
                    print(f"Aggregated privacy loss from server to client: {aggregated_eps_s_to_c}") 
                    print(f"Number of parallel traces from server to client: {video_num}") 
                    print(f"Average aggregated overhead from server to client: {total_overhead_mean}")
                    print(f"Standard deviation of aggregated overhead from server to client: {total_overhead_std}")  
               
                # Baseline results
                baseline_results['average_aggregated_overhead_constant_rate_dynamic'].append(average_aggregated_overhead_constant_rate_dynamic)
                baseline_results['std_aggregated_overhead_constant_rate_dynamic'].append(std_aggregated_overhead_constant_rate_dynamic)
                baseline_results['average_aggregated_overhead_constant_rate_anonymized'].append(average_aggregated_overhead_constant_rate_anonymized/video_num)
                baseline_results['std_aggregated_overhead_constant_rate_anonymized'].append(std_aggregated_overhead_constant_rate_anonymized/video_num)
                # TODO: needs modification
                baseline_results['average_aggregated_overhead_pacer'].append(average_aggregated_overhead_pacer)
                baseline_results['std_aggregated_overhead_pacer'].append(std_aggregated_overhead_pacer)
                baseline_results['video_num'].append(video_num)

                pbar.update(1)

    return baseline_results, results



def number_of_traces_vs_overhead_video_bidirectional(config: configlib.Config, filtered_data):
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))

    
    # From server to client data and parameters
    filtered_data_s_to_c = filtered_data[0]
    print(f"Sum of rows of filtered_data_s_to_c (max): {filtered_data_s_to_c.sum(axis=1).max()}")

    if(config.middlebox_period_server_to_client_us % config.app_time_resolution_server_to_client_us != 0):
        raise ValueError("The middlebox period must be a multiple of the application time resolution.")
    else:
        DP_step_s_to_c = int(config.middlebox_period_server_to_client_us / config.app_time_resolution_server_to_client_us) 
    
       
        
    privacy_window_size_s_to_c_us = config.privacy_window_size_server_to_client_s * 1e6
    num_of_queries_s_to_c = int(privacy_window_size_s_to_c_us / config.middlebox_period_server_to_client_us)

    filtered_data_pruned_s_to_c = filtered_data_s_to_c.drop(columns=['video_name'])

    # Baseline overhead from server to client 
    original_data_s_to_c, shaped_data_constant_rate_s_to_c = NonDP_transport(filtered_data_pruned_s_to_c, config.app_time_resolution_server_to_client_us, config.data_time_resolution_server_to_client_us, method="constant-rate") 
    constant_rate_max_s_to_c = shaped_data_constant_rate_s_to_c.max().max()
    overhead_constant_rate_dynamic_s_to_c = overhead(original_data_s_to_c, shaped_data_constant_rate_s_to_c) 

    # Pacer overhead from server to client
    original_data_s_to_c, shaped_data_pacer_s_to_c = NonDP_transport(filtered_data_s_to_c, config.app_time_resolution_server_to_client_us, config.data_time_resolution_server_to_client_us, method="pacer_video")
    overhead_pacer_s_to_c = overhead(original_data_s_to_c, shaped_data_pacer_s_to_c) 
    
    
    noise_multipliers_s_to_c = [get_noise_multiplier(privacy_loss, num_of_queries_s_to_c, alphas, config.delta_server_to_client) for privacy_loss in config.privacy_losses_server_to_client]  


    # From client to server data and parameters (if bidirectional) 
    filtered_data_c_to_s = filtered_data[1]
    # Print sum of rows of filtered_data_c_to_s
    print(f"Sum of rows of filtered_data_c_to_s (max): {filtered_data_c_to_s.sum(axis=1).max()}")
    
    if(config.middlebox_period_client_to_server_us % config.app_time_resolution_client_to_server_us != 0): 
        raise ValueError("The middlebox period must be a multiple of the application time resolution.")
    else: 
        DP_step_c_to_s = int(config.middlebox_period_client_to_server_us / config.app_time_resolution_client_to_server_us)
        
    
    privacy_window_size_c_to_s_us = config.privacy_window_size_client_to_server_s * 1e6
    num_of_queries_c_to_s = int(privacy_window_size_c_to_s_us / config.middlebox_period_client_to_server_us)
        
    filtered_data_pruned_c_to_s = filtered_data_c_to_s.drop(columns=['video_name'])

    # Baseline overhead from client to server
    original_data_c_to_s, shaped_data_constant_rate_c_to_s = NonDP_transport(filtered_data_pruned_c_to_s, config.app_time_resolution_client_to_server_us, config.data_time_resolution_client_to_server_us, method="constant-rate") 
    constant_rate_max_c_to_s = shaped_data_constant_rate_c_to_s.max().max()
    overhead_constant_rate_dynamic_c_to_s = overhead(original_data_c_to_s, shaped_data_constant_rate_c_to_s)  

    # Pacer overhead from client to server
    overhead_pacer_c_to_s = get_pacer_web_overhead(filtered_data_pruned_c_to_s)
    

    noise_multipliers_c_to_s = [get_noise_multiplier(privacy_loss, num_of_queries_c_to_s, alphas, config.delta_client_to_server) for privacy_loss in config.privacy_losses_client_to_server]  


    # Aggregation of overhead for baseline in both directions
    average_aggregated_overhead_constant_rate_dynamic, std_aggregated_overhead_constant_rate_dynamic = overhead_aggregation(overhead_df_server_to_client=overhead_constant_rate_dynamic_s_to_c,
    overhead_df_client_to_server=overhead_constant_rate_dynamic_c_to_s) 
    
    average_aggregated_overhead_constant_rate_anonymized = average_aggregated_overhead_constant_rate_dynamic * config.max_video_num

    std_aggregated_overhead_constant_rate_anonymized = std_aggregated_overhead_constant_rate_dynamic * config.max_video_num


    # Aggregation of overhead for pacer shaping in both directions
    average_aggregated_overhead_pacer, std_aggregated_overhead_pacer = overhead_aggregation(overhead_df_server_to_client=overhead_pacer_s_to_c,
                         overhead_df_client_to_server=overhead_pacer_c_to_s)



    baseline_results = {'average_aggregated_overhead_constant_rate_dynamic':[],
                        'std_aggregated_overhead_constant_rate_dynamic': [],
                        'average_aggregated_overhead_constant_rate_anonymized':[],
                        'std_aggregated_overhead_constant_rate_anonymized':[],
                        'std_aggregated_overhead_dynamic_shaping': [],
                        'average_aggregated_overhead_pacer':[],
                        'std_aggregated_overhead_pacer': [],
                        'video_num': []}    
     
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
               'video_num': [],
               'dp_interval_server_to_client_us': [],
               'dp_interval_client_to_server_us': []}
    
    # print(f"Max burst size is {constant_rate_max_s_to_c}")
    # video_nums_list = np.linspace(config.min_video_num, config.max_video_num, config.num_of_video_nums, dtype=int)
    video_nums_list = [1, 16, 128]
    with tqdm(total=len(video_nums_list)*len(noise_multipliers_s_to_c)) as pbar:
        for nm_s_to_c, nm_c_to_s in zip(noise_multipliers_s_to_c, noise_multipliers_c_to_s):
            for video_num in video_nums_list:

                max_dp_decision_s_to_c = 1000 * constant_rate_max_s_to_c

                max_dp_decision_c_to_s = 1000 * constant_rate_max_c_to_s

                
                filtered_data_parallelized_s_to_c = get_parallel_dataframe(filtered_data_pruned_s_to_c, video_num, config.max_videos_per_single_experiment)

                filtered_data_parallelized_c_to_s = get_parallel_dataframe(filtered_data_pruned_c_to_s, video_num, config.max_videos_per_single_experiment)
                
                
                original_data_s_to_c, DP_data_s_to_c, dummy_data_s_to_c = DP_transport(filtered_data_parallelized_s_to_c, config.app_time_resolution_server_to_client_us, config.transport_type, config.DP_mechanism, config.sensitivity_server_to_client, DP_step_s_to_c, config.data_time_resolution_server_to_client_us, noise_multiplier=nm_s_to_c, min_DP_size=config.min_dp_decision_server_to_client, max_DP_size=max_dp_decision_s_to_c)
                
                original_data_c_to_s, DP_data_c_to_s, dummy_data_c_to_s = DP_transport(filtered_data_parallelized_c_to_s, config.app_time_resolution_client_to_server_us, config.transport_type, config.DP_mechanism, config.sensitivity_client_to_server, DP_step_c_to_s, config.data_time_resolution_client_to_server_us, noise_multiplier=nm_c_to_s, min_DP_size=config.min_dp_decision_client_to_server, max_DP_size=max_dp_decision_c_to_s)


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
                results['video_num'].append(video_num)
                results['dp_interval_client_to_server_us'].append(config.middlebox_period_client_to_server_us)
                results['dp_interval_server_to_client_us'].append(config.middlebox_period_server_to_client_us)
               
               
                # Printing some results
                print_results = True
                if print_results:
                    print(f"DP interval {config.app_time_resolution_server_to_client_us}")
                    print(f"Aggregated privacy loss from server to client: {aggregated_eps_s_to_c}") 
                    print(f"Number of parallel traces from server to client: {video_num}") 
                    print(f"Average aggregated overhead from server to client: {total_overhead_mean}")
                    print(f"Standard deviation of aggregated overhead from server to client: {total_overhead_std}")
               
               
                
               
                # Baseline results
                baseline_results['average_aggregated_overhead_constant_rate_dynamic'].append(average_aggregated_overhead_constant_rate_dynamic)
                baseline_results['std_aggregated_overhead_constant_rate_dynamic'].append(std_aggregated_overhead_constant_rate_dynamic)
                baseline_results['average_aggregated_overhead_constant_rate_anonymized'].append(average_aggregated_overhead_constant_rate_anonymized/video_num)
                baseline_results['std_aggregated_overhead_constant_rate_anonymized'].append(std_aggregated_overhead_constant_rate_anonymized/video_num)
                # TODO: needs modification
                baseline_results['average_aggregated_overhead_pacer'].append(average_aggregated_overhead_pacer)
                baseline_results['std_aggregated_overhead_pacer'].append(std_aggregated_overhead_pacer)
                baseline_results['video_num'].append(video_num)
                
                
                
                pbar.update(1)
    return baseline_results, results 
                
                
                
                

def number_of_traces_vs_overhead_video(config: configlib.Config, filtered_data):
    if config.communication_type == "bidirectional":
        return number_of_traces_vs_overhead_video_bidirectional(config, filtered_data)
    elif config.communication_type == "unidirectional":
        return number_of_traces_vs_overhead_video_unidirectional(config, filtered_data)
    else: 
        raise ValueError("The communication type must be either bidirectional or unidirectional.")