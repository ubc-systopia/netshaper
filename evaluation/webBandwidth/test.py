import matplotlib.pyplot as plt
import numpy as np
import pickle
import os
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D



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








def plot_dp_interval_vs_overhead_video(results):
    log_scale = True
    fig, ax = plt.subplots(dpi=300, figsize=figure_size)
    markers = ['o', 's', '^']
    colors = ['tab:blue', 'tab:orange', 'tab:green' ]
    parr = []
    label_arr = []
    number_of_paralell_clients = np.sort(list(set(results["webpage_num"])))  

    
    for ind, client_num in enumerate(number_of_paralell_clients):
        # filtering data based on the number of clients
        data = filter_data_based_on_params(results, {"webpage_num": client_num})

        if log_scale:
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
    



if __name__=='__main__':
    # load the data from pickle file
    
    with open('results/dp_interval_vs_overhead_web_(2023-12-15_12-42)/results.pkl', 'rb') as f:
        data = pickle.load(f)
    
    plot_dp_interval_vs_overhead_video(data)
     
     
    # print(data)