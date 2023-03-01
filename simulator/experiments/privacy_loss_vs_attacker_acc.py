import numpy as np
from tqdm import tqdm

import configlib
from src.transport import DP_transport 
from src.utils.DL_utils import train_test_and_report_acc as BandB_attack
from src.utils.DP_utils import calculate_privacy_loss, get_noise_multiplier



def privacy_loss_vs_attacker_accuracy(config: configlib.Config, filtered_data):


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
    
        
    ## Baseline accuracy 
    baseline_results = {'attacker_accuracy': []}
    baseline_accuracies = [] 
    for i in range(config.attack_num_of_repeats):
        baseline_accuracies.append(BandB_attack(filtered_data, config.num_of_unique_streams, config.attack_num_of_epochs, config.attack_batch_size)) 
    baseline_results['attacker_accuracy'].append(np.mean(baseline_accuracies)) 


    results = {'alpha': [], 'attacker_accuracy': [], 'privacy_loss': [], 'noise_multiplier': []}
    with tqdm(total=config.privacy_loss_num * config.attack_num_of_repeats) as pbar:
        for privacy_loss in privacy_losses:
            # Calculate the noise multiplier based on the privacy loss
            noise_multiplier = get_noise_multiplier(privacy_loss, num_of_queries, alphas, config.delta)
            
            # Applying differentially private transport of data
                    # Calculating the accuracy of the attacker based on the DP data
            accuracies = []
            for i in range(config.attack_num_of_repeats):
                original_data, DP_data, dummy_data = DP_transport(filtered_data, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.sensitivity, DP_step, config.data_time_resolution_us, noise_multiplier=noise_multiplier) 
                
                accuracies.append(BandB_attack(DP_data, config.num_of_unique_streams, config.attack_num_of_epochs, config.attack_batch_size))
                pbar.update(1)
            acc = np.mean(accuracies)
            
            # Calculating the total privacy loss
            eps, best_alpha = calculate_privacy_loss(num_of_queries, alphas, noise_multiplier, config.delta)
            # print(eps)
            # print(norm_overhead(original_data, DP_data)) 
            # print("acc: " + str(acc)) 
            # Saving the results
            results['noise_multiplier'].append(noise_multiplier)
            results['privacy_loss'].append(eps)
            results['alpha'].append(best_alpha)
            results['attacker_accuracy'].append(acc)

    return baseline_results, results
