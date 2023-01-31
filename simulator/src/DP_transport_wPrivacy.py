import numpy as np
import pandas as pd


from src.modules.Queue import Queue
from src.modules.BudgetAbsorption import BudgetAbsorption
from src.utils.queue_utils import *
from src.utils.DP_utils import *


def DP_transport_wPrivacy(app, DP_step_, DP_mechanism_, epsilon_per_sample_):

  sensitivity = 1e5 #Bytes
  # print(sensitivity)
  # Initializing DP queues corresponend to the number of application streams
  DP_max_queue_size = 1e8
  DP_min_queue_size = 0
  DP_queues = []
  for i in range(app.get_num_of_streams()):
    DP_queues.append(Queue(DP_max_queue_size, DP_min_queue_size))
    
  # dpr is Decision-Publication ratio for dividing the budget between these two phases.
  dpr = 0.7
  
  # w-epsilon is the budget for each window. As we have the global epsilon for whole stream
  ## here we calculate the budget for each burst and then multiply it by the privacy 
  ## window size (DP_step_) to get the budget for each window.
  w_epsilon = epsilon_per_sample_ * DP_step_
  # print("The available epsilon for each window is: " + str(w_epsilon * (1-dpr)))
  # Initializing wPrivacy tracker for each stream
  wPrivacy_modules = []  
  for i in range(app.get_num_of_streams()):
    wPrivacy_modules.append(BudgetAbsorption(DP_step_, w_epsilon, dpr, sensitivity))
  
  network_data = []

  i = 0 

  while(not ((app.get_status() == "terminated") and (check_all_queues_empty(DP_queues)) )):
    app_step = 1
    # Push 
    if (app.get_status() == "terminated"):
      app_realtime_data = np.zeros((app.get_num_of_streams(),)) 
    else:
      app_realtime_data = app.generate_data(app_step)
    queues_push(app_realtime_data, DP_queues)

    # DP Pull
    pull_values = queues_pull_wDP(DP_queues, wPrivacy_modules, sensitivity, DP_mechanism_)

    network_data.append(pull_values)
    i += 1

  #check_BudgetAbsorption_correctness(get_all_assigned_epsilons(wPrivacy_modules), DP_step_)
  app_df = app.data_loader()   
  app_labels = app.get_all_labels()
  network_data_array = np.array(network_data).T
  network_df = pd.DataFrame(network_data_array)
  network_df['label'] = app_labels
  return app_df, network_df
