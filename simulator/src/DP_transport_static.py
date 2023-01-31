import numpy as np
import pandas as pd

from src.modules.fpa import * 
from src.utils.queue_utils import *
from src.utils.DP_utils import *


def DP_transport_static(app, DP_step_, DP_mechanism_, global_epsilon_):

  DP_step = DP_step_
  DP_mechanism = DP_mechanism_
  global_epsilon = global_epsilon_  

  app_df = app.data_loader()
  app_data = app.get_all_data()
  app_labels = app.get_all_labels()
  sensitivity = calculate_sensitivity_using_dataset(app_data)
  if(DP_mechanism_ == "Laplace"):
    network_df = FPA_mechanism(app_data, global_epsilon, sensitivity, DP_step)
  
  network_df['label'] = app_labels

  return app_df, network_df
