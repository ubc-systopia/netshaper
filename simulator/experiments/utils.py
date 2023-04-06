import configlib

from experiments.privacy_loss_vs_attacker_acc import privacy_loss_vs_attacker_accuracy
from experiments.privacy_loss_vs_overhead import privacy_loss_vs_overhead
from experiments.BandB_vs_TCN import BandB_vs_TCN 
from experiments.neighboring_analysis import neighboring_analysis


def get_experiment_function(config: configlib.Config):
    if (config.experiment == "privacy_loss_vs_overhead"):
        return privacy_loss_vs_overhead
    elif (config.experiment == "privacy_loss_vs_attacker_accuracy"):
        return privacy_loss_vs_attacker_accuracy
    elif (config.experiment == "BandB_vs_TCN"):
        return BandB_vs_TCN
    elif (config.experiment == "neighboring_analysis"):
        return neighboring_analysis
    else:
        raise NotImplementedError
    