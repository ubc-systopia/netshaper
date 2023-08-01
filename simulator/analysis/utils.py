import configlib
from analysis.overhead import overhead_analysis
from analysis.privacy import privacy_analysis
from analysis.latency import latency_analysis

def get_analysis_function(config: configlib.Config):
    if (config.analysis_type == "overhead"):
        return overhead_analysis
    elif (config.analysis_type == "privacy"):
        return privacy_analysis
    elif (config.analysis_type == "latency"):
        return latency_analysis
    else:
        raise NotImplementedError