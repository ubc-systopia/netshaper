import configlib

from experiments.empirical_privacy import empirical_privacy 
from experiments.overhead_video import overhead_video
from experiments.overhead_web import overhead_web
# from experiments.noise_multiplier_vs_privacy_web import noise_multiplier_vs_privacy_web


def get_experiment_function(config: configlib.Config):
    if (config.experiment == "empirical_privacy"):
        return empirical_privacy
    elif (config.experiment == "number_of_flows_vs_overhead_web") or (config.experiment == "dp_interval_vs_overhead_web"):
        return overhead_web
    elif (config.experiment == "number_of_flows_vs_overhead_video") or (config.experiment == "dp_interval_vs_overhead_video"):
        return overhead_video
    else:
        raise NotImplementedError("Experiment not implemented, please check the experiment name in the config file/command input.")
    