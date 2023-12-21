import numpy as np
import pandas as pd
from tqdm import tqdm
import pickle

import configlib
from src.transport import DP_transport 
from src.utils.DL_utils_PT import train_test_and_report_acc as BandB_attack
from src.utils.DP_utils import calculate_privacy_loss, get_noise_multiplier
from src.utils.TCN_utils import train_test_and_report_acc as TCN_attack
from src.data_utils.utils import data_filter_deterministic


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


def empirical_privacy(config: configlib.Config, filtered_data_list):
    results = {'privacy_loss_server_to_client': [],
               'noise_multiplier_server_to_client': [],
               'Baseline_BandB_accuracy': [],
               'Baseline_BandB_precision': [],
               'Baseline_BandB_recall': [],
               'Baseline_TCN_accuracy': [],
               'Baseline_TCN_precision': [],
               'Baseline_TCN_recall': [],
               'DP_BandB_accuracy': [],
               'DP_BandB_precision': [],
               'DP_BandB_recall': [],
               'DPTCN_accuracy': [],
               'DP_TCN_precision': [],
               'DP_TCN_recall': [],
               'noise_multiplier_server_to_client': [],
               'alpha_server_to_client': [], 
               'dp_interval_server_to_client': [],
               'sensitivity_server_to_client': [],
               'comparsion_mode': []
    } 
    


    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))

    
    privacy_window_size_s_to_c_us = config.privacy_window_size_server_to_client_s * 1e6


    for index, filtered_data in enumerate(filtered_data_list):
        # From server to client data and parameters
        filtered_data_s_to_c = filtered_data[0]
        dp_interval_s_to_c_us = config.dp_intervals_us[index][0]
        
        num_of_queries_s_to_c = int(privacy_window_size_s_to_c_us / dp_interval_s_to_c_us)
        

        ## Initializing parameters
        DP_step = 1 
        
        


        ## Traffic analysis attack on  non-shaped traffic
        
        BandB_accs, BandB_precs, BandB_recals = attack_accuracy(config, BandB_attack, filtered_data_s_to_c)




        TCN_accs, TCN_precs, TCN_recals = attack_accuracy(config, TCN_attack, filtered_data_s_to_c)

        results['Baseline_BandB_accuracy'].append(BandB_accs)
        results['Baseline_BandB_precision'].append(BandB_precs)
        results['Baseline_BandB_recall'].append(BandB_recals)
        results['Baseline_TCN_accuracy'].append(TCN_accs)
        results['Baseline_TCN_precision'].append(TCN_precs)
        results['Baseline_TCN_recall'].append(TCN_recals)
                 

        ## Traffic analysis attack on DP shaped traffic


        privacy_losses = np.linspace(config.min_privacy_loss_server_to_client, config.max_privacy_loss_server_client, config.num_privacy_loss_server_to_client)
        
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
        return results 
