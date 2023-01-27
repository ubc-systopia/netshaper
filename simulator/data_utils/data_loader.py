import pickle
import pandas as pd
from data_utils.DataProcessor import DataProcessor

def data_loader(window_size_us, data_dir = None, load_from = 'raw_data', num_classes = 4):
  if(data_dir == None):
    print("Please specify the directory of the dataset you want to load.")
    df = None
  if(load_from == 'memory'):
    file_dir = data_dir + 'data_trace_w_' + str(int(window_size_us)) + '.p'
    df = pickle.load(open(file_dir, "rb"))
  elif(load_from == 'raw_data'):
    dp = DataProcessor(data_dir, num_classes, [1, 9], ['pkt_time', 'pkt_size'])
    df = dp.aggregated_dataframe(window_size_us) 
    # Saving the dataframe in /tmp/ for future use
    pickle.dump(df, open('/tmp/data_trace_w_' + str(int(window_size_us)) + '_c_'+ str(int(num_classes)) + '.p', "wb"))
  else:
    print("Please specify the data source you want to load from. ('memory' or 'raw_data')")
    df = None
  return df