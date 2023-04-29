import numpy as np
from tqdm import tqdm
import matplotlib.pyplot as plt
import configlib


from src.transport import DP_transport, NonDP_transport
from src.utils.DL_utils_PT import train_test_and_report_acc as BandB_attack
from src.utils.DP_utils import calculate_privacy_loss, get_noise_multiplier
from src.utils.TCN_utils import train_test_and_report_acc as TCN_attack

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


def noise_multiplier_vs_privacy(config: configlib.Config, filtered_data):
    
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

    # Baseline privacy 
    
    baseline_results = {'unshaped_TCN_accuracy': [],
                        'unshaped_TCN_precision': [],
                        'unshaped_TCN_recall': [],
                        'unshaped_BandB_accuracy': [],
                        'unshaped_BandB_precision': [],
                        'unshaped_BandB_recall': [],
                        'num_of_unique_traces': []
                        }
    print("Calculating the baseline privacy")
    unshaped_TCN_accuracy, unshaped_TCN_precision, unshaped_TCN_recall = attack_accuracy(config, TCN_attack, filtered_data)

    # unshaped_BandB_accuracy, unshaped_BandB_precision, unshaped_BandB_recall = attack_accuracy(config, BandB_attack, filtered_data)
    
    baseline_results['unshaped_TCN_accuracy'].append(unshaped_TCN_accuracy)
    baseline_results['unshaped_TCN_precision'].append(unshaped_TCN_precision)
    baseline_results['unshaped_TCN_recall'].append(unshaped_TCN_recall)
    print("Baseline privacy calculated")
            
    # baseline_results['unshaped_BandB_accuracy'].append(unshaped_BandB_accuracy)
    # baseline_results['unshaped_BandB_precision'].append(unshaped_BandB_precision)
    # baseline_results['unshaped_BandB_recall'].append(unshaped_BandB_recall)

    
    results = {'noise_multiplier': [],
               'aggregated_privacy_loss': [],
               'per_query_privacy_loss': [],
               'alpha': [],
               'dp_decision_interval_us': [],
               'shaped_TCN_accuracy': [],
               'shaped_TCN_precision': [],
               'shaped_TCN_recall': [],
               'shaped_BandB_accuracy': [],
               'shaped_BandB_precision': [],
               'shaped_BandB_recall': [],
               'shaped_TCN_accuracy_fpa': [],
               'shaped_TCN_precision_fpa': [],
               'shaped_TCN_recall_fpa': [],
               'shaped_BandB_accuracy_fpa': [],
               'shaped_BandB_precision_fpa': [],
               'shaped_BandB_recall_fpa': []
               }
         
    with tqdm(total=len(noise_multipliers) ) as pbar:
        for noise_multiplier in noise_multipliers:
            
            # DP Shaping
            original_data, DP_data, dummy_data = DP_transport(filtered_data, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.sensitivity, DP_step, config.data_time_resolution_us, noise_multiplier=noise_multiplier) 

            # Calculating the total privacy loss
            aggregated_eps, best_alpha = calculate_privacy_loss(num_of_queries, alphas, noise_multiplier, config.delta)
            per_query_eps, best_alpha = calculate_privacy_loss(1, alphas, noise_multiplier, config.delta)

            # Saving the results
            results['noise_multiplier'].append(noise_multiplier)
            results['aggregated_privacy_loss'].append(aggregated_eps)
            results['per_query_privacy_loss'].append(per_query_eps)
            results['alpha'].append(best_alpha)
            results['dp_decision_interval_us'].append(config.data_time_resolution_us)
            
            # Calculating the empirical privacy of the DP transport
            shaped_TCN_accuracy, shaped_TCN_precision, shaped_TCN_recall = attack_accuracy(config, TCN_attack, DP_data)
            
            # shaped_BandB_accuracy, shaped_BandB_precision, shaped_BandB_recall = attack_accuracy(config, BandB_attack, DP_data)
            
             
            # FPA Shaping
            original_data, FPA_data, dummy_data = DP_transport(filtered_data, config.app_time_resolution_us, "DP_static", config.DP_mechanism, config.sensitivity, DP_step, config.data_time_resolution_us, noise_multiplier=noise_multiplier)
            
            # # Calculating the empirical privacy of the FPA transport
            # shaped_TCN_accuracy_fpa , shaped_TCN_precision_fpa, shaped_TCN_recall_fpa = attack_accuracy(config, TCN_attack, FPA_data)
            
            # shaped_BandB_accuracy_fpa, shaped_BandB_precision_fpa, shaped_BandB_recall_fpa = attack_accuracy(config, BandB_attack, FPA_data)
            
            
            
            results['shaped_TCN_accuracy'].append(shaped_TCN_accuracy)
            results['shaped_TCN_precision'].append(shaped_TCN_precision) 
            results['shaped_TCN_recall'].append(shaped_TCN_recall)

            # results['shaped_BandB_accuracy'].append(shaped_BandB_accuracy)
            # results['shaped_BandB_precision'].append(shaped_BandB_precision)
            # results['shaped_BandB_recall'].append(shaped_BandB_recall)
            
            # results['shaped_TCN_accuracy_fpa'].append(shaped_TCN_accuracy_fpa)
            # results['shaped_TCN_precision_fpa'].append(shaped_TCN_precision_fpa)
            # results['shaped_TCN_recall_fpa'].append(shaped_TCN_recall_fpa)

            # results['shaped_BandB_accuracy_fpa'].append(shaped_BandB_accuracy_fpa)
            # results['shaped_BandB_precision_fpa'].append(shaped_BandB_precision_fpa)
            # results['shaped_BandB_recall_fpa'].append(shaped_BandB_recall_fpa) 

            pbar.update(1)
    
    return baseline_results, results
    