import numpy as np
import pandas as pd
import configlib
import os
import warnings
warnings.filterwarnings("ignore")

from src.data_utils.utils import get_data_saver_fn,  get_data_filter_function, get_data_loader_fn, get_data_pruner_fn, get_result_saver_fn

from experiments.utils import get_experiment_function
     
from src.plots.utils import get_plot_fn


def main():
    # load the config file
    config = configlib.parse(save_fname="tmp.txt")

    # load the data
    data_loader_fn = get_data_loader_fn(config) 
    
    '''
    The simulator supports two communication types:
    1. unidirectional
    2. bidirectional
    In bidirectional, the data is a tuple of two dataframe, one for each direction.
    In unidirectional, the data is a tuple with the first element being the dataframe and the second element being None. 
    ''' 
    data_list = data_loader_fn(config) 
    
    '''
    We cache the data of the experiment in the directory specified by config.save_data_dir. in case the data is already cached, we load it from the cache. 
    ''' 
    if config.save_data_dir is not None:
        data_saver_fn = get_data_saver_fn(config)
        data_saver_fn(config, data_list)

    
    '''
    Some experiments requires to have the title of each webpage/video in the dataset, others don't. Here, we prune the data to only include the required columns based on the experiment type. 
    '''      
    data_pruner_fn = get_data_pruner_fn(config)  
    data_list = data_pruner_fn(config, data_list)

    
    '''
    Some experiments requires to filter the data based on some criteria. Here, we filter the data based on the experiment requirements. 
    ''' 
    data_filter_fn = get_data_filter_function(config)
    filtered_data_list = data_filter_fn(config, data_list)


    '''
    Run the experiment. 
    '''
    experiment_fn = get_experiment_function(config)
    results = experiment_fn(config, filtered_data_list)


    '''
    Save the results of the experiment. 
    '''
    result_saver_fn = get_result_saver_fn(config)
    result_saver_fn(config, results)

    
    '''
    Plot the data
    '''
    plot_fn = get_plot_fn(config)
    plot_fn(config, results)
    
    

     
        
            
if __name__ == '__main__':
  main()