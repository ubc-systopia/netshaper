import numpy as np
import pandas as pd
import configlib


from src.transport import DP_transport, NonDP_transport
from src.data_utils.data_loader import data_loader, data_saver

def main():
  # load the config file
  config = configlib.parse(save_fname="tmp.txt")
  
  # if(config.middlebox_period_us % config.app_time_resolution_us != 0):
  #   print("The middlebox period must be a multiple of the application time resolution.")
  #   return -1
  # else:
  #   DP_step = int(config.middlebox_period_us / config.app_time_resolution_us)



  
  # original_data, DP_data, dummy_data = DP_transport(data, config.app_time_resolution_us, config.transport_type, config.DP_mechanism, config.epsilon_per_query, DP_step, config.data_time_resolution_us)

  data_time_resolution_us_list = [2e6, 1e6, 5e5, 1e5, 5e4]
  
  for data_time_resolution_us in data_time_resolution_us_list:
    data = data_loader(data_time_resolution_us, config.load_data_dir, config.data_source, config.num_of_unique_streams)
    
    data_saver(data, config.save_data_dir, data_time_resolution_us, config.num_of_unique_streams)   
    print("Data saved for data_time_resolution_us = ", data_time_resolution_us)
  

   
   
   
        
if __name__ == '__main__':
  main()