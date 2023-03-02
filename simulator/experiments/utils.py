import configlib

from experiments.privacy_loss_vs_attacker_acc import privacy_loss_vs_attacker_accuracy
from experiments.privacy_loss_vs_overhead import privacy_loss_vs_overhead


def get_experiment_function(config: configlib.Config):
    if (config.experiment == "privacy_loss_vs_overhead"):
        return privacy_loss_vs_overhead
    elif (config.experiment == "privacy_loss_vs_attacker_accuracy"):
        return privacy_loss_vs_attacker_accuracy
    else:
        raise NotImplementedError
    