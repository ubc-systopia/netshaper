import configlib

from experiments.empirical_privacy import empirical_privacy 
from experiments.number_of_traces_vs_overhead_video import number_of_traces_vs_overhead_video
from experiments.number_of_traces_vs_overhead_web import number_of_traces_vs_overhead_web
# from experiments.noise_multiplier_vs_privacy_web import noise_multiplier_vs_privacy_web


def get_experiment_function(config: configlib.Config):
    if (config.experiment == "empirical_privacy"):
        return empirical_privacy
    elif (config.experiment == "number_of_traces_vs_overhead_web"):
        return number_of_traces_vs_overhead_web
    elif (config.experiment == "number_of_traces_vs_overhead_video"):
        return number_of_traces_vs_overhead_video
    elif (config.experiment == "dp_interval_vs_overhead_web"):
        return dp_interval_vs_overhead_web
    elif (config.experiment == "dp_interval_vs_overhead_video"):
        return dp_interval_vs_overhead_video
    else:
        raise NotImplementedError
    