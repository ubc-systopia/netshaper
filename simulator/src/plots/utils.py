import matplotlib.pyplot as plt
import numpy as np
import os
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D


from src.data_utils.utils import ensure_dir




plt.rcParams['pdf.fonttype'] = 42
plt.rcParams['ps.fonttype'] = 42
plt.rcParams.update({'font.size': 14})
figure_size=(5,2.0)


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




def plot_overhead_comparison_web_loglog(config, results):
    # Rounding the values of aggregated privacy loss to 2 decimal points
    if config.communication_type == 'unidirectional': 
        results['aggregated_privacy_loss'] = [round(x, 2) for x in results['aggregated_privacy_loss']]
    elif config.communication_type == 'bidirectional':
        results['aggregated_privacy_loss'] = [round(x, 2) for x in results['aggregated_privacy_loss_server_to_client']]

    privacy_losses_of_interest = [1, 4, 16]

    fig, ax = plt.subplots(dpi=300, figsize=(5, 2.2))

    markers = ['^', 's', 'o']
    colors = ['tab:blue', 'tab:orange', 'tab:green'] 
    
    legend_handles = []
    legend_labels = []
    marker_indices = [0, 1, 10 , 99]      
    for ind, aggregated_privacy_loss in enumerate(privacy_losses_of_interest):

        # Filtering data based on aggregated_privacy_loss
        fixed_params = {'aggregated_privacy_loss': aggregated_privacy_loss}
        simulator_scalability_data_dp_filtered = filter_data_based_on_params(results, fixed_params)
       
        number_of_webpages =  simulator_scalability_data_dp_filtered['webpage_num']
        average_overhead_dp = simulator_scalability_data_dp_filtered['average_aggregated_overhead']
        
        # getting the crossing point
        pacer_average_overhead_tmp = results['average_aggregated_overhead_pacer']
        
        ax.loglog(number_of_webpages, average_overhead_dp, color=colors[ind],
                label=f'NS, $\epsilon_W$={aggregated_privacy_loss}') 
        
        marker_style = markers[ind]
        plt.plot([number_of_webpages[i] for i in marker_indices], [average_overhead_dp[i] for i in marker_indices], marker_style,
                    markerfacecolor=colors[ind], markeredgecolor=colors[ind], markeredgewidth=1.5, markersize=8)
        
        legend_handle = Line2D([0], [0], linestyle='-', linewidth=2, color=colors[ind],
                                marker=marker_style, markersize=8,
                                markerfacecolor=colors[ind], markeredgecolor=colors[ind],
                                markeredgewidth=1.5)
        legend_handles.append(legend_handle)
        legend_labels.append(f'NS, $\epsilon_W$={aggregated_privacy_loss}') 
    
    # Plotting baseline

    ax.loglog(results['webpage_num'], results['average_aggregated_overhead_pacer'], label='Pacer', color='red', linestyle='--') 
    legend_handle = Line2D([0], [0], linestyle='--', linewidth=2, color='red')
    legend_handles.append(legend_handle)
    legend_labels.append('Pacer')
    
    
    ax.loglog(results['webpage_num'], results['average_aggregated_overhead_constant_rate_anonymized'], label='Constant Rate', color='black', linestyle='dotted')
    legend_handle = Line2D([0], [0], linestyle='dotted', linewidth=2, color='black')
    legend_handles.append(legend_handle)
    legend_labels.append('CR')
     
    ax.set_ylabel('Per-flow overhead')
    ax.set_xlabel('Number of concurrent flows') 
    # handles, labels = ax.get_legend_handles_labels()
    legend1 = ax.legend(legend_handles[:3], legend_labels[:3], framealpha=0, handlelength=1, fontsize=12, loc=(0.01, 0.01), ncol=1)

    # Create the second legend for the last two items
    legend2 = ax.legend(legend_handles[3:], legend_labels[3:], framealpha=0, handlelength=1, fontsize=12, loc=(0.37, 0.01), ncol=1)

    # Add the legends to the plot
    ax.add_artist(legend1)
    ax.add_artist(legend2)
    plt.yticks([10**i for i in range(-4, 6, 2)])
    plt.ylim(bottom=15**-4)
    # plt.xlim(right=1100)
    # plt.xticks([i for i in range(0, 1201, 200)])
    plt.title('$\delta_W$=1e-6, $\Delta_W$=60 KB, $T$=50 ms')
    plot_dir = config.plot_dir
    ensure_dir(plot_dir)
    plot_file = os.path.join(plot_dir, "overhead_comparsion_web_loglog.pdf")
    plt.savefig(plot_file, bbox_inches='tight', transparent=True)
    plt.close() 
        


def plot_overhead_comparison_video_loglog(config, results):
    # Rounding the values of aggregated privacy loss to 2 decimal points
    if config.communication_type == 'unidirectional':
        results['aggregated_privacy_loss'] = [round(x, 2) for x in results['aggregated_privacy_loss']]
    elif config.communication_type == 'bidirectional':
        results['aggregated_privacy_loss'] = [round(x, 2) for x in results['aggregated_privacy_loss_server_to_client']]    
        
        
    privacy_losses_of_interest = [1, 4, 16]


    fig, ax = plt.subplots(dpi=300, figsize=(5, 2.2))
    
    markers = ['^', 's', 'o']
    colors = ['tab:blue', 'tab:orange', 'tab:green']
    
    legend_handles = []
    legend_labels = []
    marker_indices = [0, 1, 10 , 99]
    for ind, aggregated_privacy_loss in enumerate(privacy_losses_of_interest):

        # Filtering data based on aggregated_privacy_loss
        fixed_params = {'aggregated_privacy_loss': aggregated_privacy_loss}
        simulator_scalability_data_dp_filtered = filter_data_based_on_params(results, fixed_params)
       
        number_of_videos =  simulator_scalability_data_dp_filtered['video_num']
        average_overhead_dp = simulator_scalability_data_dp_filtered['average_aggregated_overhead']
        
        # getting the crossing point
        pacer_average_overhead_tmp = results['average_aggregated_overhead_pacer']
        
        ax.loglog(number_of_videos, average_overhead_dp,
                color=colors[ind],label=f'NS, $\epsilon_W$={aggregated_privacy_loss}') 

        marker_style = markers[ind]
        marker_freq = 100  # Number of data points between each marker
        # print(len(number_of_videos), len(average_overhead_dp))
        plt.plot([number_of_videos[i] for i in marker_indices], [average_overhead_dp[i] for i in marker_indices], marker_style,
                 markerfacecolor=colors[ind], markeredgecolor=colors[ind], markeredgewidth=1.5, markersize=8)
        
        # Create legend handles with markers
        legend_handle = Line2D([0], [0], linestyle='-', linewidth=2, color=colors[ind],
                              marker=marker_style, markersize=8,
                              markerfacecolor=colors[ind], markeredgecolor=colors[ind],
                              markeredgewidth=1.5)
        legend_handles.append(legend_handle)
        
        # Create legend labels
        legend_labels.append(f'NS, $\epsilon_W$={aggregated_privacy_loss}')
    
    # Plotting baseline
    ax.loglog(results['video_num'], results['average_aggregated_overhead_pacer'], label='Pacer', color='red', linestyle='--')
    legend_handle = Line2D([0], [0], linestyle='--', linewidth=2, color='red')
    legend_handles.append(legend_handle)
    legend_labels.append('Pacer')
     
    # ax.loglog(simulator_scalability_data_baseline['video_num'], simulator_scalability_data_baseline['average_aggregated_overhead_constant_rate_dynamic'], label='Constant Rate', color='black', linestyle='dotted') 
    ax.loglog(results['video_num'], results['average_aggregated_overhead_constant_rate_anonymized'], label='Constant Rate', color='black', linestyle='dotted') 
    legend_handle = Line2D([0], [0], linestyle='dotted', linewidth=2, color='black') 
    legend_handles.append(legend_handle)
    legend_labels.append('CR') 

    ax.set_ylabel('Per-flow overhead')
    ax.set_xlabel('Number of concurrent flows') 
    plt.ylim(bottom=10**-5)
    plt.yticks([10**i for i in range(-4, 6, 2)])
    plt.xticks([10**i for i in range(0, 4)])

    legend1 = ax.legend(legend_handles[:3], legend_labels[:3], framealpha=0, handlelength=1, fontsize=12, loc=(0.01, 0.01), ncol=1)

    # Create the second legend for the last two items
    legend2 = ax.legend(legend_handles[3:], legend_labels[3:], framealpha=0, handlelength=1, fontsize=12, loc=(0.40, 0.01), ncol=1)

    # Add the legends to the plot
    ax.add_artist(legend1)
    ax.add_artist(legend2)
    plt.title('$\delta_W$=1e-6, $\Delta_W$=2.5 MB, $T$=1 s')
    plt.savefig(f'figures/overhead_vs_number_of_traces_video_{config.gcommunication_type}_loglog_updated.pdf', bbox_inches='tight', transparent=True)
    plt.close()

    
def plot_dp_interval_vs_overhead_web(config, results):
    fig, ax = plt.subplots(dpi=300, figsize=(5,2.2))
    markers = ['o', 's', '^']
    colors = ['tab:blue', 'tab:orange', 'tab:green' ]
    parr = []
    label_arr = []
    for ind, client_num in enumerate(data_handwritten_web.keys()):
        plt.plot(data_handwritten_web[client_num]['dp_interval_ms'], data_handwritten_web[client_num]['overhead_avg'], label=f'{client_num} clients', markersize=8, marker=markers[ind], color=colors[ind]) 
        plt.errorbar(data_handwritten_web[client_num]['dp_interval_ms'],
                data_handwritten_web[client_num]['overhead_avg'], yerr=data_handwritten_web[client_num]['overhead_std'],
                capsize=3, color=colors[ind], linestyle='None')
    
    plt.legend(framealpha=0, handlelength=1, fontsize=12, ncol=2, loc=(0.27, 0.65))
    plt.xlabel('DP interval, $T$ (ms)')
    plt.ylabel('Per-flow overhead')
    plt.yticks([i for i in range(0, 26, 5)])
    plt.xticks([i for i in range(0, 101, 20)])
    plt.title('$\epsilon_W$=1, $\delta_W$=1e-6, $\Delta_W$=60 KB')
    plt.savefig('figures/overhead_vs_dp_interval_web_updated.pdf', bbox_inches='tight', transparent=True)
    plt.close() 
 


def plot_dp_interval_vs_overhead_web(config, results):
    logscale = config.plot_logscale
    fig, ax = plt.subplots(dpi=300, figsize=figure_size)
    markers = ['o', 's', '^']
    colors = ['tab:blue', 'tab:orange', 'tab:green' ]
    parr = []
    label_arr = []
    number_of_paralell_clients = np.sort(list(set(results["webpage_num"])))  

    
    for ind, client_num in enumerate(number_of_paralell_clients):
        # filtering data based on the number of clients
        data = filter_data_based_on_params(results, {"webpage_num": client_num})

        if logscale:
            plt.semilogy(np.array(data['dp_interval_server_to_client_us'])/1e3, data['average_aggregated_overhead'], label=f'{client_num} clients', markersize=8, marker=markers[ind], color=colors[ind])
            
            
        else:
            plt.plot(np.array(data['dp_interval_server_to_client_us'])/1e3, data['average_aggregated_overhead'], label=f'{client_num} clients', markersize=8, marker=markers[ind], color=colors[ind])

        plt.errorbar(np.array(data['dp_interval_server_to_client_us'])/1e3,data['average_aggregated_overhead'], yerr=data['std_aggregated_overhead'], capsize=3, color=colors[ind], linestyle='None')
        
        
    
    plt.legend(framealpha=0, handlelength=1, fontsize=12, ncol=1)
    plt.xlabel('DP interval, $T$ (ms)')
    plt.ylabel('Latency (ms)')
    # plt.yticks([i for i in range(0, 3001, 1000)])
    plt.xticks([i for i in range(0, 101, 20)])
    plt.title('$\epsilon_W$=1, $\delta_W$=1e-6, $\Delta_W$=2.5 MB')
    plt.savefig('figures/latency_vs_dp_interval_video_updated.pdf', bbox_inches='tight', transparent=True)
    plt.close()
    




def get_plot_fn(config):
    # TODO: Make the plot function separated from the experiment for better modularity
    if config.experiment == "privacy_loss_vs_noise_std":
        return lambda x, y: None
    elif config.experiment == "privacy_loss_vs_query_num":
        return lambda x, y: None
    elif config.experiment == "overhead_comparison_web":
        return plot_overhead_comparison_web_loglog
    elif config.experiment == "overhead_comparison_video":
        return plot_overhead_comparison_video_loglog    
    elif config.experiment == "dp_interval_vs_overhead_web":
        return plot_dp_interval_vs_overhead_web
    elif config.experiment == "dp_interval_vs_overhead_video":
        return plot_dp_interval_vs_overhead_video
    else:
        return lambda x, y: None 
           


