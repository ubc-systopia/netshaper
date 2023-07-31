import os


import numpy as np
import pandas as pd

from src.modules.Application import  Application
from src.DP_transport_dynamic import DP_transport_dynamic
from src.DP_transport_static import DP_transport_static
from src.DP_transport_wPrivacy import DP_transport_wPrivacy

from src.utils.overhead_utils import *



def DP_transport(data, app_time_resolution_us, transport_type, DP_mechanism, sensitivity, DP_step_, data_time_resolution_us, epsilon_per_query = 1, noise_multiplier = 1, verbose=False, global_epsilon = 1, min_DP_size = 0, max_DP_size = 1e6):
  ## Create the application with a predefined application window size (the timing resolution at the application level)

  DP_step = DP_step_


  
  app = Application(app_time_resolution_us, data, data_time_resolution_us)

  if(transport_type == "DP_wPrivacy"):
    return DP_transport_wPrivacy(app, DP_step, DP_mechanism,  epsilon_per_query, sensitivity)
  elif(transport_type == "DP_dynamic"):
    return DP_transport_dynamic(app, DP_step, DP_mechanism, epsilon_per_query, sensitivity, noise_multiplier,  min_DP_size, max_DP_size, verbose)
  elif(transport_type == "DP_static"):
    return DP_transport_static(app, global_epsilon, sensitivity)
  else:
    return -1, -1
  

def NonDP_transport(data, app_time_resolution_us, data_time_resolution_us, method="constant-rate"):
    app = Application(app_time_resolution_us, data, data_time_resolution_us)
    app_df = app.data_loader()
    app_labels = app.get_all_labels()
    app_data = app.get_all_data()
    if (method == "constant-rate"):
        max_value = (app_data.max()).max()
        print("Max queue size is: " + str(max_value))
        reshaped_data = [[max_value] * len(app_data.columns)] * len(app_data)
        reshaped_data = pd.DataFrame(reshaped_data) 
        reshaped_df = pd.concat([reshaped_data, app_labels], axis=1)
        print(reshaped_data.head)
        return app_df, reshaped_df
    elif (method == "pacer_video"):
        # Getting the set of video names that are important for us
        video_names = data['video_name'].unique() 
        video_names = [ x[x.index('mpd-')+4:x.index('.csv')] for x in video_names]
        # Getting Pacer data 
        #TODO: make it configurable
        pacer_data = pd.read_csv("/home/ubuntu/data/pacer/tvseries_720p_sizes.csv")
        pacer_data.fillna(0, inplace=True)

        # Filter based on video names
        pacer_data['Folder'] = (pacer_data['Folder'].astype(str)).apply(lambda x: x[x.index('yt-')+3:x.index('-frag')])
        pacer_data = pacer_data[pacer_data['Folder'].isin(video_names)]
        # Map the video name to labels from 0 to n
        pacer_data['Folder'] = pacer_data['Folder'].astype('category').cat.codes
        # Rename the Folder column to label
        pacer_data.rename(columns={'Folder': 'label'}, inplace=True)
        # Move the label column to the end
        column_to_move = pacer_data.pop('label')
        pacer_data.insert(len(pacer_data.columns), column_to_move.name, column_to_move) 
        
        app_data = pacer_data.drop(['label'], axis=1)
        app_labels = pacer_data['label'].reset_index()
        max_values = app_data.max()
        replicated_series = pd.Series(np.tile(max_values, (len(app_data),1)).T.flatten())
        reshaped_data = replicated_series.values.reshape((len(app_data), len(max_values)), order='F')
        reshaped_data = pd.DataFrame(reshaped_data)
        reshaped_df = pd.concat([reshaped_data, app_labels], axis=1)
        # Sum of all the values in the pacer data
        pacer_sum = pacer_data.sum(axis=1).sum()
        reshaped_sum = reshaped_df.sum(axis=1).sum()
        print("overhead is: " + str(reshaped_sum/pacer_sum))
        return pacer_data, reshaped_df
    elif (method == "pacer_web"):
        pass
        