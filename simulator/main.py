import numpy as np
import pandas as pd

from modules import  Application
from DP_transport_dynamic import DP_transport_dynamic
from DP_transport_static import DP_transport_static
from DP_transport_wPrivacy import DP_transport_wPrivacy

from utils.overhead_utils import *



def DP_transport(app_window_size_, DP_setup_,DP_mechanism_,  epsilon_per_sample_, DP_step_, verbose=True):
  ## Create the application with a predefined application window size (the timing resolution at the application level)

  app_window_size = app_window_size_
  DP_setup = DP_setup_
  DP_mechanism = DP_mechanism_
  DP_step = DP_step_
  epsilon_per_sample = epsilon_per_sample_

  app = Application(app_window_size)
  if(DP_setup == "wPrivacy"):
    return DP_transport_wPrivacy(app, DP_step, DP_mechanism, epsilon_per_sample)
  elif(DP_setup == "dynamic"):
    return DP_transport_dynamic(app, DP_step, DP_mechanism, epsilon_per_sample, verbose)
  elif(DP_setup == "static"):
    return DP_transport_static(app, DP_step, DP_mechanism, epsilon_per_sample)
  else:
    return -1, -1
  

def NonDP_transport(app_window_size_):
  app_window_size = app_window_size_
  app = Application(app_window_size)

  app_df = app.data_loader()
  app_labels = app.get_all_labels()
  app_data = app.get_all_data()
  max_value = (app_data.max()).max()
  reshaped_data = [[max_value] * len(app_data.columns)] * len(app_data)
  reshaped_data = pd.DataFrame(reshaped_data) 
  reshaped_df = pd.concat([reshaped_data, app_labels], axis=1)
  return app_df, reshaped_df