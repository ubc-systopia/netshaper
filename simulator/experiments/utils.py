import configlib
import numpy as np


from experiments.empirical_privacy import empirical_privacy 
from experiments.overhead_video import overhead_video
from experiments.overhead_web import overhead_web
from experiments.privacy_microbenchmarks import privacy_loss_VS_noise_std, privacy_loss_VS_query_num


def sort_x_based_on_y(x, y): 
    x_sorted_indices = np.argsort(x)
    x_sorted = np.array(x)[x_sorted_indices]
    y_sorted = np.array(y)[x_sorted_indices]
    return x_sorted, y_sorted
def all_params_match(data, params, i):
    for key in params.keys():
        if(data[key][i] != params[key]):
            return False
    return True


def filter_data_based_on_params(data, params):
    filtered_data = {key: [] for key in data.keys()}
    # going through all iteration of the parameters in the data
    for i in range(len(data[next(iter(data.keys()))])):
        if((all_params_match(data, params, i))):
            for key in data.keys():
                filtered_data[key].append(data[key][i])
    return filtered_data 





def get_experiment_function(config: configlib.Config):
    if (config.experiment == "empirical_privacy"):
        return empirical_privacy
    elif (config.experiment == "number_of_flows_vs_overhead_web") or (config.experiment == "dp_interval_vs_overhead_web"):
        return overhead_web
    elif (config.experiment == "number_of_flows_vs_overhead_video") or (config.experiment == "dp_interval_vs_overhead_video"):
        return overhead_video
    elif (config.experiment == "privacy_loss_vs_noise_std"):
        return privacy_loss_VS_noise_std
    elif (config.experiment == "privacy_loss_vs_query_num"):
        return privacy_loss_VS_query_num
    else:
        raise NotImplementedError("Experiment not implemented, please check the experiment name in the config file/command input.")
    