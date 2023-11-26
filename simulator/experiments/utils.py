import configlib
import numpy as np


from experiments.empirical_privacy import empirical_privacy 
from experiments.overhead_video import overhead_video
from experiments.overhead_web import overhead_web
from experiments.privacy_microbenchmarks import privacy_loss_VS_noise_std, privacy_loss_VS_query_num







def get_experiment_function(config: configlib.Config):
    if (config.experiment == "empirical_privacy"):
        return empirical_privacy
    elif (config.experiment == "overhead_comparison_web") or (config.experiment == "dp_interval_vs_overhead_web"):
        return overhead_web
    elif (config.experiment == "overhead_comparison_video") or (config.experiment == "dp_interval_vs_overhead_video"):
        return overhead_video
    elif (config.experiment == "privacy_loss_vs_noise_std"):
        return privacy_loss_VS_noise_std
    elif (config.experiment == "privacy_loss_vs_query_num"):
        return privacy_loss_VS_query_num
    else:
        raise NotImplementedError("Experiment not implemented, please check the experiment name in the config file/command input.")
    