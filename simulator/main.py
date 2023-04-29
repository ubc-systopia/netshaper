import numpy as np
import pandas as pd
import configlib
import os
import warnings
warnings.filterwarnings("ignore")

from src.data_utils.data_loader import data_loader_experimental, data_loader_realistic, data_saver, experiment_result_saver, analysis_result_saver, get_data_filter_function, prune_data

from experiments.utils import get_experiment_function
from analysis.utils import get_analysis_function
from src.data_utils.testbed_data_processor import aggregated_data_list   
     
def main():
    # load the config file
    config = configlib.parse(save_fname="tmp.txt")
    
    if config.run_type == "experiment":
        # Load the data
        if config.setup == "realistic":
            data = data_loader_realistic(config.data_time_resolution_us, config.load_data_dir, config.data_source, config.max_num_of_unique_streams, config.max_num_of_traces_per_stream)
        elif config.setup == "experimental":
            data = data_loader_experimental(config.data_time_resolution_us, config.load_data_dir, config.data_source, config.max_num_of_unique_streams)
        else:
            raise ValueError("Please specify the setup you want to run. ('realistic' or 'experimental')")
        
        if config.save_data_dir is not None:
            data_saver(data, config.save_data_dir, config.data_time_resolution_us, config.max_num_of_unique_streams)

        
        # Filtering the data
        data = prune_data(config, data)
        if config.filtering_type is not None:
            data_filter_fn = get_data_filter_function(config)
            filtered_data = data_filter_fn(config, data)
        else:
            filtered_data = data
        # Running the experiment 
        experiment_fn = get_experiment_function(config)
        baseline_results, results = experiment_fn(config, filtered_data)
        # Saving the results
        experiment_result_saver(config, results, baseline_results)
    elif config.run_type == "analysis":
        #TODO: implement analysis
        data_list = aggregated_data_list(config)
        analysis_fn = get_analysis_function(config)
        results = analysis_fn(config, data_list)
        analysis_result_saver(config, results)
    else: 
        raise ValueError("Please specify the run_type you want to run. ('experiment' or 'analysis'")
     

     
        
            
if __name__ == '__main__':
  main()