import configlib
from analysis.overhead import overhead_analysis
from analysis.privacy import privacy_analysis


def get_analysis_function(config: configlib.Config):
    if (config.analysis_type == "overhead"):
        return overhead_analysis
    elif (config.analysis_type == "privacy"):
        return privacy_analysis
    else:
        raise NotImplementedError