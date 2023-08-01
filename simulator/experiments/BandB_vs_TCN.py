import numpy as np
import pandas as pd
from tqdm import tqdm
import pickle

import configlib
from src.transport import DP_transport 
from src.utils.DL_utils_PT import train_test_and_report_acc as BandB_attack
from src.utils.DP_utils import calculate_privacy_loss, get_noise_multiplier
from src.utils.TCN_utils import train_test_and_report_acc as TCN_attack
from src.data_utils.data_loader import data_filter_deterministic


def get_attack_fn(config: configlib.Config):
    if config.attack_model == "seq_mnist_tcn":
        return TCN_attack
    elif config.attack_model == "burst_and_beauty":
        return BandB_attack
    else:
        raise NotImplementedError("The attack model is not implemented")


def attack_accuracy(config:configlib.Config, attack_fn, DP_data):
    accs = []
    precs = []
    recals = []
    for i in range(config.attack_num_of_repeats):
        # original_data, DP_data, dummy_data = DP_transport(filtered_data, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.sensitivity, DP_step, config.data_time_resolution_us, noise_multiplier=noise_multiplier)  
        acc,  prec, recal = attack_fn(config, DP_data)
        accs.append(acc)
        precs.append(prec)
        recals.append(recal)

    return accs, precs, recals


def BandB_vs_TCN(config: configlib.Config, datasets: pd.DataFrame):
    data = datasets[0]
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
       
    ## Baseline accuracy 
    if (config.comparison_mode == "baseline"):
        baseline_results = {'classes_num': [], 'TCN_accuracy': [], 'TCN_precision': [], 'TCN_recall': [], 'BandB_accuracy': [], 'BandB_precision': [], 'BandB_recall': []}
        classes_num_list = np.linspace(config.min_classes_num, config.max_classes_num, config.classes_num_num, dtype=int)
        with tqdm(total=len(classes_num_list)) as pbar:
            for classes_num in classes_num_list:
                pbar.set_description(f'Number of videos: {classes_num} ...')
                # Filtering data based on the config
                config.num_of_unique_streams =  classes_num
                filtered_data = data_filter_deterministic(config, df = data)
                baseline_results['classes_num'].append(classes_num)
                
                BandB_accs, BandB_precs, BandB_recals = attack_accuracy(config, BandB_attack, filtered_data)
                baseline_results['BandB_accuracy'].append(BandB_accs)
                baseline_results['BandB_precision'].append(BandB_precs)
                baseline_results['BandB_recall'].append(BandB_recals)
                
                 
                TCN_accs, TCN_precs, TCN_recals = attack_accuracy(config, TCN_attack, filtered_data)
                baseline_results['TCN_accuracy'].append(TCN_accs)
                baseline_results['TCN_precision'].append(TCN_precs)
                baseline_results['TCN_recall'].append(TCN_recals) 
                pbar.update(1)    
                
        return baseline_results, {}
    
    if (config.comparison_mode == "DP"):
        privacy_losses = np.linspace(config.privacy_loss_min, config.privacy_loss_max, config.privacy_loss_num)
        
        filtered_data = data_filter_deterministic(config, df = data) 

        results = {'privacy_loss': [], 'noise_multiplier': [], 'BandB_accuracy': [], 'BandB_precision': [], 'BandB_recall': [], 'TCN_accuracy': [], 'TCN_precision': [], 'TCN_recall': []}
        with tqdm(total=len(privacy_losses)) as pbar: 
            for privacy_loss in privacy_losses:
                pbar.set_description(f'Privacy loss: {privacy_loss} ...')
                noise_multiplier = get_noise_multiplier(privacy_loss, num_of_queries, alphas, config.delta) 

                original_data, DP_data, dummy_data = DP_transport(filtered_data, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.sensitivity, DP_step, config.data_time_resolution_us, noise_multiplier=noise_multiplier)   
                # Calculating the total privacy loss
                eps, best_alpha = calculate_privacy_loss(num_of_queries, alphas, noise_multiplier, config.delta)
                
                results['privacy_loss'].append(eps)
                results['noise_multiplier'].append(noise_multiplier)

                BandB_accs, BandB_precs, BandB_recals = attack_accuracy(config, BandB_attack, DP_data)
                results['BandB_accuracy'].append(BandB_accs)
                results['BandB_precision'].append(BandB_precs)
                results['BandB_recall'].append(BandB_recals)
                
                 
                TCN_accs, TCN_precs, TCN_recals = attack_accuracy(config, TCN_attack, DP_data)
                results['TCN_accuracy'].append(TCN_accs)
                results['TCN_precision'].append(TCN_precs)
                results['TCN_recall'].append(TCN_recals)  
                pbar.update(1)                    
        return {}, results 