import numpy as np
import pandas as pd
import configlib
import wandb
import os

from src.data_utils.data_loader import data_loader, data_saver, data_filter_stochastic, data_filter_deterministic, result_saver

from experiments.utils import get_experiment_function
    
     
def main():
    # load the config file
    config = configlib.parse(save_fname="tmp.txt")
   
    # Load the data
    data = data_loader(config.data_time_resolution_us, config.load_data_dir, config.data_source, config.max_num_of_unique_streams)
    
    # Filtering data based on the config
    filtered_data = data_filter_deterministic(config, df = data)
    
    # Running the experiment 
    experiment_fn = get_experiment_function(config)
    baseline_results, results = experiment_fn(config, filtered_data)
    
    # Saving the results
    result_saver(config, results, baseline_results) 
     

     
        
            
if __name__ == '__main__':
  main()