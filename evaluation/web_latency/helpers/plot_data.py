import numpy as np
import matplotlib.pyplot as plt
import os
import pickle
import argparse







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



def plot_latency_vs_dp_interval(data, results_dir):
    client_nums = np.unique(data["client_num"])
    fig, ax = plt.subplots(dpi=300, figsize=(5, 2))
    markers = ['o', 's', '^']
    colors = ['tab:blue', 'tab:orange', 'tab:green' ]
    parr = []
    ind = 0
    for client_num in client_nums:
        fixed_params = {"client_num": client_num}        
        filtered_data = filter_data_based_on_params(data, fixed_params)
        dp_intervals = np.array(filtered_data["dp_intevals"])/1e3
        mean = np.array(filtered_data["mean"])/1e3
        std = np.array(filtered_data["std"])/1e3
        
        mean, dp_intervals = sort_x_based_on_y(mean, dp_intervals)
        plt.plot(dp_intervals, mean, label=f"{client_num} clients", markersize=8, marker=markers[ind], color=colors[ind])
        plt.errorbar(dp_intervals, mean, yerr=std, capsize=3, color=colors[ind], linestyle='None')
        ind += 1 
    plt.legend(framealpha=0, handlelength=1, fontsize=12, ncol=1)
    plt.xlabel('DP interval, $T$ (ms)')
    plt.ylabel('Latency (ms)')
    # plt.yticks([i for i in range(0, 101, 20)])
    plt.xticks([i for i in range(0, 101, 20)])
    plt.title('$\epsilon_W$=1, $\delta_W$=1e-6, $\Delta_W$=60 KB')
    plot_file = os.path.join(results_dir, "latency_vs_dp_interval.pdf")
    plt.savefig(plot_file, bbox_inches='tight', transparent=True)
    plt.close()

def main():
    parser = argparse.ArgumentParser(description="Description of your program")
    
    # Add arguments
    parser.add_argument("--results_dir", type=str, help="Path to the directory containing the results")
    

    args = parser.parse_args()
    # check for the required arguments
    if args.results_dir is None:
        parser.error("Please provide the required arguments")
    else: 
        results_dir = args.results_dir
        



    data_dir = os.path.join(results_dir, "processed_data.pkl")
    
    # Load the processed data
    with open(data_dir, "rb") as file:
        processed_data = pickle.load(file)
    
    print(processed_data)  
    plot_latency_vs_dp_interval(processed_data, results_dir)    




if __name__=="__main__":
    main()