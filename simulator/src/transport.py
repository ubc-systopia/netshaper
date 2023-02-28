import numpy as np
import pandas as pd

from src.modules.Application import  Application
from src.DP_transport_dynamic import DP_transport_dynamic
from src.DP_transport_static import DP_transport_static
from src.DP_transport_wPrivacy import DP_transport_wPrivacy

from src.utils.overhead_utils import *



def DP_transport(data, app_time_resolution_us, transport_type, DP_mechanism, sensitivity, DP_step_, data_time_resolution_us, epsilon_per_query = 1, noise_multiplier = 1, verbose=True):
  ## Create the application with a predefined application window size (the timing resolution at the application level)

  DP_step = DP_step_


  
  app = Application(app_time_resolution_us, data, data_time_resolution_us)

  if(transport_type == "DP_wPrivacy"):
    return DP_transport_wPrivacy(app, DP_step, DP_mechanism,  epsilon_per_query, sensitivity)
  elif(transport_type == "DP_dynamic"):
    return DP_transport_dynamic(app, DP_step, DP_mechanism, epsilon_per_query, sensitivity, noise_multiplier, verbose)
  elif(transport_type == "DP_static"):
    return DP_transport_static(app, DP_step, DP_mechanism, epsilon_per_query)
  else:
    return -1, -1
  

def NonDP_transport(data, app_time_resolution_us, data_time_resolution_us):
  app = Application(app_time_resolution_us, data, data_time_resolution_us)
  app_df = app.data_loader()
  app_labels = app.get_all_labels()
  app_data = app.get_all_data()
  max_value = (app_data.max()).max()
  reshaped_data = [[max_value] * len(app_data.columns)] * len(app_data)
  reshaped_data = pd.DataFrame(reshaped_data) 
  reshaped_df = pd.concat([reshaped_data, app_labels], axis=1)
  return app_df, reshaped_df