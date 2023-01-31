import numpy as np
import pandas as pd

from src.utils.queue_utils import *
from src.utils.DP_utils import *
from src.modules.Queue import Queue


def DP_transport_dynamic(app, DP_step_, DP_mechanism_, epsilon_per_sample_, verbose=True):
  DP_max_queue_size = 1e8
  DP_min_queue_size = 0
  DP_queues = []
  for i in range(app.get_num_of_streams()):
    DP_queues.append(Queue(DP_max_queue_size, DP_min_queue_size))

  network_data = []
  dummy_data = []
  ## Implementing our DP mechanism logic 
  k = app.get_stream_len()/DP_step_
  if(verbose): 
    print("Number of updates is: " + str(k))
  epsilon_per_update = epsilon_per_sample_ * DP_step_

  if(verbose):
    print("epsilon per update is: " + str(epsilon_per_update))
  #sensitivity = calculate_sensitivity_using_dataset(app.get_all_data())
  sensitivity = 1e5 #Bytes
  if(verbose):
    print("The sensitivity of dataset is: " + str(sensitivity))
  i = 0
  while(not ((app.get_status() == "terminated") and (check_all_queues_empty(DP_queues)) )):
    # App step is number of windows application push together to the transport layer queue
    app_step = 1
    DP_step = DP_step_
    # Push 
    if (app.get_status() == "terminated"):
      app_realtime_data = np.zeros((app.get_num_of_streams(),)) 
    else:
      app_realtime_data = app.generate_data(app_step)
    queues_push(app_realtime_data, DP_queues)

    # DP Pull
    if(i%DP_step == 0):
      push_values, dummy_values = queues_pull_DP(DP_queues, epsilon_per_update, sensitivity, DP_mechanism_)
    
    network_data.append(push_values)
    dummy_data.append(dummy_values)
    i += 1
    #print(check_all_queues_empty(DP_queues).sum())
    #if(i > 120):
    #  print(get_queue_sizes(DP_queues))

  app_df = app.data_loader()   
  app_labels = app.get_all_labels()
  network_data_array = np.array(network_data).T
  network_df = pd.DataFrame(network_data_array)
  network_df['label'] = app_labels
  dummy_data_array = np.array(dummy_data).T
  dummy_df = pd.DataFrame(dummy_data_array)
  dummy_df['label'] = app_labels
  return app_df, network_df, dummy_df