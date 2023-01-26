import pandas as pd
from data_utils.DataProcessor import DataProcessor
from data_utils.data_loader import data_loader


window_size_us = 1e6

raw_data_directory_100 = '/home/ubuntu/data/raw_datasets/video/220508-1105/NIC1000-base/pervid_niter/'
raw_data_directory_4 = '/home/ubuntu/data/raw_datasets/video/220327-1017/NIC1000-base/pervid_niter/'
processed_data_directory_4 = '/home/ubuntu/data/windowed_stream_4/'
processed_data_directory_100 = '/home/ubuntu/data/windowed_stream_100/'


 
df = data_loader(window_size_us, processed_data_directory_100, 'memory')
df = data_loader(window_size_us, raw_data_directory_100, 'raw_data', 4)
print(df)

