import numpy as np
import pandas as pd
import configlib
import os
from tqdm import tqdm
import datetime
import pickle
from scipy.optimize import fsolve
# import wandb
import os


from src.transport import DP_transport, NonDP_transport
from src.data_utils.data_loader import data_loader, data_saver, data_filter_stochastic, data_filter_deterministic
from src.utils.overhead_utils import norm_overhead, wasserstein_overhead
from src.utils.DL_utils import train_test_and_report_acc
from src.utils.DP_utils import calculate_privacy_loss, get_noise_multiplier


# os.environ['WANDB_API_KEY']='3bc497370717aefd365ca72cfffffced761fd80d'
# os.environ['WANDB_ENTITY']='amirsabzi'
# wandb.init(project="test-project")



def advanced_composition_root(epsilon, *args):
  global_epsilon, delta, k = args
  return (np.sqrt(2*k*np.log(1/(delta))) * epsilon + (k * epsilon)*(np.exp(epsilon)-1)) - global_epsilon

def advanced_composition(epsilon, delta, k):
  first_half = np.sqrt(2*k*np.log(1/(delta))) * epsilon
  print(first_half)
  second_half = (k * epsilon)*(np.exp(epsilon)-1)
  print(second_half)
  return first_half + second_half


def main():
  # load the config file
  config = configlib.parse(save_fname="tmp.txt")
#   wandb.config = {
#     "test_var_1" : 0.01,
#     "test_var_2": 0.01
#   } 
  
  
   
  if(config.middlebox_period_us % config.app_time_resolution_us != 0):
    print("The middlebox period must be a multiple of the application time resolution.")
    return -1
  else:
    DP_step = int(config.middlebox_period_us / config.app_time_resolution_us)
    
  # Loading the data
  data = data_loader(config.data_time_resolution_us, config.load_data_dir, config.data_source, config.max_num_of_unique_streams) 
    
#   data_saver(data, config.save_data_dir, config.data_time_resolution_us, config.num_of_unique_streams)

  # Filtering the data
  if (config.filtering_type == "deterministic"):
    filtered_data = data_filter_deterministic(data, config.num_of_unique_streams, config.num_of_traces_per_stream, config.max_num_of_unique_streams, config.max_num_of_traces_per_stream)
    
  elif (config.filtering_type == "stochastic"):
    filtered_data = data_filter_stochastic(data, config.num_of_unique_streams, config.num_of_traces_per_stream, config.max_num_of_unique_streams, config.max_num_of_traces_per_stream)
  else: 
    print("The filtering type is not supported.")
    return
      
  print(advanced_composition(3.399, config.delta, 100))
  # Running the experiment 
  if (config.experiment == "global_epsilon_VS_attacker_accuracy"): 
    # Initializing the parameters of experiment
    privacy_window_size_us = config.privacy_window_size_s * 1e6
    num_of_queries = int(privacy_window_size_us / config.middlebox_period_us)
    
    global_epsilons = np.linspace(config.global_epsilon_min, config.global_epsilon_max, config.global_epsilon_num)
    
    results = {'global_epsilon': [], 'attacker_accuracy': []} 
    # Getting the accuracy of the attacker based on the original data
    results['global_epsilon'].append(0)
    baseline_accuracies = []
    for i in range(config.attack_num_of_repeats):
      baseline_accuracies.append(train_test_and_report_acc(filtered_data, config.num_of_unique_streams, config.attack_num_of_epochs, config.attack_batch_size))
    results['attacker_accuracy'].append(np.mean(baseline_accuracies))

    with tqdm(total=config.global_epsilon_num * config.attack_num_of_repeats) as pbar:
      for global_epsilon in global_epsilons:
        # Calculating the epsilon per query we have based on the global epsilon
        epsilon_per_query = fsolve(advanced_composition_root, 0.1, args=(global_epsilon, config.delta, num_of_queries))
        
        # Applying differentially private transport of data
        original_data, DP_data, dummy_data = DP_transport(filtered_data, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.sensitivity, DP_step, config.data_time_resolution_us, epsilon_per_query=epsilon_per_query) 
        
        # Calculating the accuracy of the attacker based on the DP data
        accuracies = []
        for i in range(config.attack_num_of_repeats):
          accuracies.append(train_test_and_report_acc(DP_data, config.num_of_unique_streams, config.attack_num_of_epochs, config.attack_batch_size))
          pbar.update(1)
        acc = np.mean(accuracies)
        # Saving the results
        results['global_epsilon'].append(global_epsilon)
        results['attacker_accuracy'].append(acc)
  
  if(config.experiment == "global_epsilon_VS_overhead"):    
    # Initializing the parameters of experiment
    privacy_window_size_us = config.privacy_window_size_s * 1e6
    num_of_queries = int(privacy_window_size_us / config.middlebox_period_us)
    
    global_epsilons = np.linspace(config.global_epsilon_min, config.global_epsilon_max, config.global_epsilon_num) 
    
    results = {'global_epsilon': [], 'average_aggregated_overhead': [], 'average_norm_distance': [], 'average_wasserstein_distance': []} 
    
    with tqdm(total=config.global_epsilon_num) as pbar:
      for global_epsilon in global_epsilons:
        epsilon_per_query = fsolve(advanced_composition_root, 0.1, args=(global_epsilon, config.delta, num_of_queries))
        
        # Applying differentially private transport of data
        original_data, DP_data, dummy_data = DP_transport(filtered_data, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.sensitivity, DP_step, config.data_time_resolution_us, epsilon_per_query=epsilon_per_query) 
        
        # Calculating the overhead of the DP transport
        _, average_aggregated_overhead, _, _, average_norm_distance, _ = norm_overhead(original_data, DP_data)
        
        wasserstein_dist = wasserstein_overhead(original_data, DP_data)
        
        results['global_epsilon'].append(global_epsilon)
        results['average_aggregated_overhead'].append(average_aggregated_overhead)
        results['average_norm_distance'].append(average_norm_distance)
        results['average_wasserstein_distance'].append(wasserstein_dist)
        
        pbar.update(1) 
  
  if(config.experiment == "privacy_loss_VS_attacker_accuracy"): 
    privacy_window_size_us = config.privacy_window_size_s * 1e6
    num_of_queries = int(privacy_window_size_us / config.middlebox_period_us)
    
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))   
     
    privacy_losses = np.linspace(config.privacy_loss_min, config.privacy_loss_max, config.privacy_loss_num)
    
    baseline_accuracies = [] 
    for i in range(config.attack_num_of_repeats):
      baseline_accuracies.append(train_test_and_report_acc(filtered_data, config.num_of_unique_streams, config.attack_num_of_epochs, config.attack_batch_size)) 
    print("baseline acc:" + str(np.mean(baseline_accuracies)))  
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
        
          accuracies.append(train_test_and_report_acc(DP_data, config.num_of_unique_streams, config.attack_num_of_epochs, config.attack_batch_size))
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
        
  if(config.experiment == "privacy_loss_VS_overhead"): 
    privacy_window_size_us = config.privacy_window_size_s * 1e6
    num_of_queries = int(privacy_window_size_us / config.middlebox_period_us)
    
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))   
     
    privacy_losses = np.linspace(config.privacy_loss_min, config.privacy_loss_max, config.privacy_loss_num)
    
    # Calculating the overhead of constant shaping     
    original_data, shaped_data = NonDP_transport(filtered_data, config.app_time_resolution_us, config.data_time_resolution_us) 
    
    _, average_aggregated_overhead, _, _, average_norm_distance, _ = norm_overhead(original_data, shaped_data)
    print("aggregated_overhead", average_aggregated_overhead) 
    print("norm", average_norm_distance)
    print("wasserstein", wasserstein_overhead(original_data, shaped_data))
    
    results = {'privacy_loss': [], 'noise_multiplier': [],  'average_aggregated_overhead': [], 'average_norm_distance': [], 'average_wasserstein_distance': [], 'alpha': []} 
              
    with tqdm(total=config.privacy_loss_num ) as pbar:
      for privacy_loss in privacy_losses:
        # Calculate the noise multiplier based on the privacy loss
        noise_multiplier = get_noise_multiplier(privacy_loss, num_of_queries, alphas, config.delta)
        
        # Applying differentially private transport of data
        original_data, DP_data, dummy_data = DP_transport(filtered_data, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.sensitivity, DP_step, config.data_time_resolution_us, noise_multiplier=noise_multiplier) 
        
   
        
        # Calculating the total privacy loss
        eps, best_alpha = calculate_privacy_loss(num_of_queries, alphas, noise_multiplier, config.delta)
        # print(eps)
        # print(norm_overhead(original_data, DP_data)) 
        # print("acc: " + str(acc)) 
        # Saving the results
        results['noise_multiplier'].append(noise_multiplier)
        results['privacy_loss'].append(eps)
        results['alpha'].append(best_alpha)
        
        # Calculating the overhead of the DP transport
        _, average_aggregated_overhead, _, _, average_norm_distance, _ = norm_overhead(original_data, DP_data)
        
        wasserstein_dist = wasserstein_overhead(original_data, DP_data)
        
        results['average_aggregated_overhead'].append(average_aggregated_overhead)
        results['average_norm_distance'].append(average_norm_distance)
        results['average_wasserstein_distance'].append(wasserstein_dist)
        pbar.update(1)
    
        # wandb.log({"aggregated_overhead" : average_aggregated_overhead}) 
     
  # Saving the results in results directory
  ## check if the results directory exists
  if not os.path.exists(config.results_dir):
    os.mkdir(config.results_dir)
  
  ## create a directory basded on the experiment name and date and time
  now = datetime.datetime.now()
  child_dir = config.results_dir + config.experiment + "_(" + now.strftime("%Y-%m-%d_%H-%M") + ")/"
  os.mkdir(child_dir)

  ## save the config file in the results directory
  with open(child_dir + "config.json", "w") as f:
    f.write(str(config))
    
  pickle.dump(results, open(child_dir + "results.pkl", "wb")) 

if __name__ == "__main__":
  main()