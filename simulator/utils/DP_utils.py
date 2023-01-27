import numpy as np
import pandas as pd
from queue_utils import *



def calculate_sensitivity_using_dataset(app_data):
  max_colBased = app_data.max()
  min_colBased = app_data.min()
  diff = max_colBased - min_colBased
  print(len(diff))
  sensitivity = max(diff.to_numpy())
  return sensitivity

def laplace_mechanism(value, l1_sensitivity, epsilon):
  mu = 0
  gamma = l1_sensitivity/epsilon
  DP_value = value + np.random.laplace(mu, gamma, 1)[0]
  return DP_value

def gaussian_mechanism(value, l2_sensitivity, epsilon, delta=1e-5):
  mu = 0
  sigma = (l2_sensitivity/epsilon) * np.sqrt(2*np.log(1.25/delta))
  DP_value = value + np.random.normal(mu, sigma, 1)[0]
  return DP_value

def queues_pull_DP(queues_list, epsilon, sensitivity, DP_mechanism):
  DP_data_sizes = []
  dummy_sizes = []
  for i in range(len(queues_list)):
    true_size = queues_list[i].get_size()

    # Making DP decision about dequeue size
    #DP_size = true_size + random.choice([-10,0])
    if(DP_mechanism == "Gaussian"):
      DP_size = gaussian_mechanism(true_size, sensitivity, epsilon, 1e-5)
    elif(DP_mechanism == "Laplace"):
      DP_size = laplace_mechanism(true_size, sensitivity, epsilon)
    DP_size = 0 if DP_size < 0 else DP_size
    DP_data_sizes.append(DP_size)

    # Updating queue size
    if(DP_size > true_size):
      queues_list[i].dequeue(true_size)
      dummy_size = DP_size - true_size
    else:
      queues_list[i].dequeue(DP_size)
      dummy_size = 0
    dummy_sizes.append(dummy_size)
  return DP_data_sizes, dummy_sizes

def queues_pull_wDP(queues_list, wPrivacy_modules, sensitivity, DP_mechanism):
  DP_data_sizes = []
  for i in range(len(queues_list)):
    true_size = queues_list[i].get_size()

    # Making DP decision about queue size
    if(DP_mechanism == "Gaussian"):
      pass
    elif(DP_mechanism == "Laplace"):
      DP_size = wPrivacy_modules[i].make_query_wDP(true_size)  

    DP_size = 0 if DP_size < 0 else DP_size
    DP_data_sizes.append(DP_size)

    # Updating queue size
    if(DP_size > true_size):
      queues_list[i].dequeue(true_size)
    else:
      queues_list[i].dequeue(DP_size)
  return DP_data_sizes
  
def check_BudgetAbsorption_correctness(epsilons_df, w):
  upper_bound_indx = len(epsilons_df.columns) - w - 1
  w_epsilons = []
  for i in range(upper_bound_indx):
    w_epsilon = epsilons_df.iloc[:,i:i+w].sum(axis=1)
    w_epsilons.append(list(w_epsilon))
    
def get_all_assigned_epsilons(wPrivacy_modules):
  epsilons_df = []
  for wPrivacy_module in wPrivacy_modules:
    epsilons_df.append(wPrivacy_module.get_assigned_epsilons())
  epsilons_df = pd.DataFrame(epsilons_df)
  return epsilons_df


# def FPA_mechanism(app_data, epsilon, sensitivity, k):
#   nonPrivate_arr = app_data.to_numpy()
#   private_arr = []
#   for idx, line in enumerate(nonPrivate_arr):
#     retQ = list(fpa(line, epsilon, sensitivity, k)) 
#     private_arr.append(retQ)
#   private_df = pd.DataFrame(private_arr)
#   return private_df