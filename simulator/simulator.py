import numpy as np
import pandas as pd
import configlib
import os
import warnings
warnings.filterwarnings("ignore")

from src.data_utils.utils import data_loader, data_saver, experiment_result_saver, analysis_result_saver, get_data_filter_function, prune_data

from experiments.utils import get_experiment_function
from analysis.utils import get_analysis_function
from src.data_utils.testbed_data_processor import get_data_aggregator_function   
     
def main():
    # load the config file
    config = configlib.parse(save_fname="tmp.txt")
    '''
    The simulator supports two communication types:
    1. unidirectional
    2. bidirectional
    
    In bidirectional, the data is a tuple of two dataframe, one for each direction.
    In unidirectional, the data is a tuple with the first element being the dataframe and the second element being None. 
    ''' 
    data_list = data_loader(config) 
    
    '''
    We cache the data of the experiment in the directory specified by config.save_data_dir. in case the data is already cached, we load it from the cache. 
    ''' 
    if config.save_data_dir is not None:
        data_saver(config, data_list)

    
    '''
    Some experiments requires to have the title of each webpage/video in the dataset, others don't. Here, we prune the data to only include the required columns based on the experiment type. 
    '''        
    data = prune_data(config, data)

    
    '''
    Some experiments requires to filter the data based on some criteria. Here, we filter the data based on the experiment requirements. 
    ''' 
    data_filter_fn = get_data_filter_function(config)
    filtered_data_list = data_filter_fn(config, data_list)

    experiment_fn = get_experiment_function(config)
    
    results = experiment_fn(config, filtered_data_list)
    experiment_result_saver(config, results)

    

     
        
            
if __name__ == '__main__':
  main()