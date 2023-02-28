import numpy as np
import pandas as pd
import configlib
import wandb
import os

from src.transport import DP_transport, NonDP_transport
from src.data_utils.data_loader import data_loader, data_saver, data_filter_stochastic, data_filter_deterministic
from src.utils.overhead_utils import norm_overhead, wasserstein_overhead
from src.utils.DL_utils import train_test_and_report_acc


os.environ['WANDB_API_KEY']='3bc497370717aefd365ca72cfffffced761fd80d'
os.environ['WANDB_ENTITY']='amirsabzi'
wandb.init(project="test-project")


def main():
  # load the config file
  config = configlib.parse(save_fname="tmp.txt")
  
  if(config.middlebox_period_us % config.app_time_resolution_us != 0):
    print("The middlebox period must be a multiple of the application time resolution.")
    return -1
  else:
    DP_step = int(config.middlebox_period_us / config.app_time_resolution_us)


  # Load the data
  data = data_loader(config.data_time_resolution_us, config.load_data_dir, config.data_source, config.num_of_unique_streams)
  

  filtered_data = data_filter_deterministic(data, 10, 60, 100, 100)
   
  original_data, DP_data, dummy_data = DP_transport(filtered_data, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.epsilon_per_query, config.sensitivity, DP_step, config.data_time_resolution_us)
  
  print(norm_overhead(filtered_data, DP_data))
  print(wasserstein_overhead(filtered_data, DP_data)) 
  print(train_test_and_report_acc(DP_data, 10, 100, 100)) 
        
if __name__ == '__main__':
  main()