import configlib

from experiments.privacy_loss_vs_attacker_acc import privacy_loss_vs_attacker_accuracy
from experiments.privacy_loss_vs_overhead import privacy_loss_vs_overhead
from experiments.BandB_vs_TCN import BandB_vs_TCN 
from experiments.neighboring_analysis_web_video import neighboring_analysis
from experiments.noise_multiplier_vs_overhead_video import noise_multiplier_vs_overhead_video
from experiments.noise_multiplier_vs_privacy_video import noise_multiplier_vs_privacy_video
from experiments.number_of_traces_vs_overhead_video import number_of_traces_vs_overhead_video
from experiments.noise_multiplier_vs_overhead_web import noise_multiplier_vs_overhead_web
from experiments.number_of_traces_vs_overhead_web import number_of_traces_vs_overhead_web
# from experiments.noise_multiplier_vs_privacy_web import noise_multiplier_vs_privacy_web


def get_experiment_function(config: configlib.Config):
    if (config.experiment == "privacy_loss_vs_overhead"):
        return privacy_loss_vs_overhead
    elif (config.experiment == "privacy_loss_vs_attacker_accuracy"):
        return privacy_loss_vs_attacker_accuracy
    elif (config.experiment == "noise_multiplier_vs_overhead_video"):
        return noise_multiplier_vs_overhead_video
    elif (config.experiment == "noise_multiplier_vs_privacy_video"):
        return noise_multiplier_vs_privacy_video
    elif (config.experiment == "BandB_vs_TCN"):
        return BandB_vs_TCN
    elif (config.experiment == "neighboring_analysis_web"):
        return neighboring_analysis
    elif (config.experiment == "neighboring_analysis_video"):
        return neighboring_analysis
    elif (config.experiment == "number_of_traces_vs_overhead_web"):
        return number_of_traces_vs_overhead_web
    elif (config.experiment == "number_of_traces_vs_overhead_video"):
        return number_of_traces_vs_overhead_video
    elif (config.experiment == "noise_multiplier_vs_overhead_web"):
        return noise_multiplier_vs_overhead_web
    elif (config.experiment == "noise_multiplier_vs_privacy_web"):
        return noise_multiplier_vs_privacy_video
        return  
    else:
        raise NotImplementedError
    