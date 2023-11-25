import numpy as np
import matplotlib.pyplot as plt
import pickle
import pandas as pd
import os
from matplotlib.lines import Line2D
from matplotlib.patches import Patch
from matplotlib.legend_handler import HandlerTuple

import configlib
from src.utils.DP_utils import calculate_privacy_loss, get_noise_multiplier
from src.data_utils.utils import ensure_dir

    
def privacy_loss_VS_query_num(config: configlib.Config, data: None):
    # noise_variances, delta, sensitivity
    noise_variances = config.noise_variances
    delta = config.delta
    sensitivity = config.sensitivity
    plot_path = config.plot_dir 
    results_path = config.results_dir
    
      
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))   
    query_nums = np.linspace(1, 64, 64)
    # plt.figure(dpi=300, figsize=figure_size)
    

    fig, ax = plt.subplots(dpi=300, figsize=(5,2.2))
    
    
    markers = ['^', 's', 'o', '+']
    colors = ['tab:blue', 'tab:orange', 'tab:green', 'tab:red']
    
    legend_handles = []
    legend_labels = []
    
    results = {"sensitivity": sensitivity, "delta": delta, "noise_variances": noise_variances, "query_nums": query_nums, "privacy_losses": []}

    for ind, noise_variance in enumerate(noise_variances):
        noise_multiplier = noise_variance * 1/sensitivity
        epsilons = np.array([ calculate_privacy_loss(query_num, alphas, noise_multiplier, delta)[0] for query_num in query_nums ])
        results["privacy_losses"].append(epsilons)
        plt.semilogy(query_nums, epsilons, color=colors[ind], linewidth=2, label=f'$\sigma$: {noise_variance/1e6} MB')
        
        marker_style = markers[ind]
        marker_freq = 10  # Number of data points between each marker
        plt.plot(query_nums[::marker_freq], epsilons[::marker_freq], marker_style,
                 markerfacecolor=colors[ind], markeredgecolor=colors[ind], markeredgewidth=1.5, markersize=8)
        
        # Create legend handles with markers
        legend_handle = Line2D([0], [0], linestyle='-', linewidth=2, color=colors[ind],
                              marker=marker_style, markersize=8,
                              markerfacecolor=colors[ind], markeredgecolor=colors[ind],
                              markeredgewidth=1.5)
        legend_handles.append(legend_handle)
        
        # Create legend labels
        legend_labels.append(f'$\sigma_T$ = {noise_variance/1e6} MB')

    plt.xlabel('# of DP queries, $N$')
    plt.ylabel('Privacy loss, $\epsilon_W$')
    # plt.xlim(0, 129)
    # plt.xticks([ i for i in range(0, 71, 10) ])
    # plt.ylim(bottom=10**-1, top=2*10**2)
    # plt.yticks([ 10**i for i in range(-1, 6, 2) ])


    # Create custom legend for markers
    marker_legend1 = plt.legend(legend_handles[:2], legend_labels[:2], framealpha=0,
                               handlelength=1, fontsize=12, loc=(0.24, 0.005), ncol=1)
    
    marker_legend2 =plt.legend(legend_handles[2:], legend_labels[2:], framealpha=0,
                               handlelength=1, fontsize=12, loc=(0.63, 0.005), ncol=1)
    # Add the custom legend to the plot
    ax.add_artist(marker_legend1)
    ax.add_artist(marker_legend2)


    plt.title(f'$\delta_W$={delta}, $\Delta_W$={sensitivity/1e6} MB')
    ensure_dir(plot_path)
    plot_full_path = os.path.join(plot_path, "privacy_loss_VS_query_num.pdf")
    plt.savefig(plot_full_path, bbox_inches='tight', transparent=True)
    plt.close()
    
    # saving the results
    ensure_dir(results_path)
    results_file = os.path.join(results_path, "privacy_loss_VS_query_num.pkl")
    with open(results_file, "wb") as f:
        pickle.dump(results_file, f)




def privacy_loss_VS_noise_std(config: configlib.Config, data: None):
    #  sensitivity, delta, num_of_queries
    sensitivity = config.sensitivity
    delta = config.delta
    num_of_queries = config.num_of_queries
    plot_path = config.plot_dir 
    results_path = config.results_dir
    
            
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))   
    stds = np.linspace(2*100e3, 64*100e3, 100)
    
    fig, ax = plt.subplots(dpi=300, figsize=(5, 2.2))
    
    markers = ['^', 's', 'o', '+']
    colors = ['tab:blue', 'tab:orange', 'tab:green', 'tab:red']
    
    legend_handles = []
    legend_labels = []
    
    results = {"sensitivity": sensitivity, "delta": delta, "num_of_queries": num_of_queries, "stds": stds, "privacy_losses": []} 
     
    for ind, query_num in enumerate(num_of_queries):
        noise_multipliers = stds/sensitivity
        epsilons = np.array([calculate_privacy_loss(query_num, alphas, noise_multiplier, delta)[0]
                            for noise_multiplier in noise_multipliers])
        
        plt.semilogy(stds/1e6, epsilons, color=colors[ind], linewidth=2,
                label=f'$N$ = {query_num}')
        results["privacy_losses"].append(epsilons) 
        marker_style = markers[ind]
        marker_freq = 10  # Number of data points between each marker
        plt.plot(stds[::marker_freq]/1e6, epsilons[::marker_freq], marker_style,
                 markerfacecolor=colors[ind], markeredgecolor=colors[ind], markeredgewidth=1.5, markersize=8)
        
        # Create legend handles with markers
        legend_handle = Line2D([0], [0], linestyle='-', linewidth=2, color=colors[ind],
                              marker=marker_style, markersize=8,
                              markerfacecolor=colors[ind], markeredgecolor=colors[ind],
                              markeredgewidth=1.5)
        legend_handles.append(legend_handle)
        
        # Create legend labels
        legend_labels.append(f'$N$ = {query_num}')
        
    plt.xlabel('Noise, $\sigma_{T}$ (MB)')
    plt.ylabel('Privacy loss, $\epsilon_W$')
    # plt.xlim(0, 7)
    # plt.xticks([i for i in range(8)])
    # plt.yticks([10**i for i in range(0, 5)])
    
    
    # Create custom legend for markers
    marker_legend1 = plt.legend(legend_handles[:2], legend_labels[:2], framealpha=0,
                               handlelength=1, fontsize=12, loc=(0.27, 0.65), ncol=1)
    
    marker_legend2 =plt.legend(legend_handles[2:], legend_labels[2:], framealpha=0,
                               handlelength=1, fontsize=12, loc=(0.64, 0.65), ncol=1)
    # Add the custom legend to the plot
    ax.add_artist(marker_legend1)
    ax.add_artist(marker_legend2)

    
    plt.title(f'$\delta_W$={delta}, $\Delta_W$={sensitivity/1e6} MB')
    ensure_dir(plot_path)
    plot_path_full = os.path.join(plot_path, "privacy_loss_VS_noise_std.pdf")
    plt.savefig(plot_path_full, bbox_inches='tight', transparent=True)
    plt.close()
    
    # saving the results
    ensure_dir(results_path)
    results_file = os.path.join(results_path, "privacy_loss_VS_noise_std.pkl")
    with open(results_file, "wb") as f:
        pickle.dump(results_file, f)
        
    
    
    
    
    