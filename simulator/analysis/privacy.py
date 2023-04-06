import numpy as np
from tqdm import tqdm

import configlib
from src.transport import DP_transport 
from src.utils.DL_utils_PT import train_test_and_report_acc as BandB_attack
from src.utils.DP_utils import calculate_privacy_loss
from src.utils.TCN_utils import train_test_and_report_acc as TCN_attack




def attack_accuracy(config:configlib.Config, attack_fn, data):
    accs = []
    precs = []
    recals = []
    for i in range(config.attack_num_of_repeats):
        # original_data, DP_data, dummy_data = DP_transport(filtered_data, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.sensitivity, DP_step, config.data_time_resolution_us, noise_multiplier=noise_multiplier)  
        acc,  prec, recal = attack_fn(config, data)
        accs.append(acc)
        precs.append(prec)
        recals.append(recal)

    return accs, precs, recals

def privacy_analysis(config: configlib.Config, data_list):
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64)) 

    results = {'noise_multiplier': [], 'privacy_loss': [], 'TCN_accuracy_shaped': [], 'TCN_precision_shaped': [], 'TCN_recall_shaped': [], 'BandB_accuracy_shaped': [], 'BandB_precision_shaped': [], 'BandB_recall_shaped': [],
    'TCN_accuracy_unshaped': [], 'TCN_precision_unshaped': [], 'TCN_recall_unshaped': [], 'BandB_accuracy_unshaped': [], 'BandB_precision_unshaped': [], 'BandB_recall_unshaped': [],
    'DP_interval_length_us': [], 'send_interval_length_us': []} 
    
    with tqdm(total=len(data_list)) as pbar:
        for data in data_list:
            results['noise_multiplier'].append(data['noise_multiplier'])
            
            steps = int(config.privacy_window_length_s * 1e6 / data['DP_interval_length_us']) 
            results['privacy_loss'].append(calculate_privacy_loss(steps, alphas, data['noise_multiplier'], config.delta))     

            TCN_acc_shaped, TCN_prec_shaped, TCN_recal_shaped = attack_accuracy(config, TCN_attack, data['shaped_data'])
            results['TCN_accuracy_shaped'].append(TCN_acc_shaped)
            results['TCN_precision_shaped'].append(TCN_prec_shaped)
            results['TCN_recall_shaped'].append(TCN_recal_shaped)
            
            BandB_acc_shaped, BandB_prec_shaped, BandB_recal_shaped = attack_accuracy(config, BandB_attack, data['shaped_data']) 
            results['BandB_accuracy_shaped'].append(BandB_acc_shaped)
            results['BandB_precision_shaped'].append(BandB_prec_shaped)
            results['BandB_recall_shaped'].append(BandB_recal_shaped)
            
            TCN_acc_unshaped, TCN_prec_unshaped, TCN_recal_unshaped = attack_accuracy(config, TCN_attack, data['unshaped_data'])
            results['TCN_accuracy_unshaped'].append(TCN_acc_unshaped)
            results['TCN_precision_unshaped'].append(TCN_prec_unshaped)
            results['TCN_recall_unshaped'].append(TCN_recal_unshaped)
            
            BandB_acc_unshaped, BandB_prec_unshaped, BandB_recal_unshaped = attack_accuracy(config, BandB_attack, data['unshaped_data'])
            results['BandB_accuracy_unshaped'].append(BandB_acc_unshaped)
            results['BandB_precision_unshaped'].append(BandB_prec_unshaped)
            results['BandB_recall_unshaped'].append(BandB_recal_unshaped)
            
            results['DP_interval_length_us'].append(data['DP_interval_length_us'])
            results['send_interval_length_us'].append(data['send_interval_length_us'])
            pbar.update(1) 
        
    return results 
