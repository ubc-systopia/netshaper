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








def plot_throughput_vs_client_num(data, results_dir):

    req_size = np.unique(data["req_size_NS"])
    fig, ax = plt.subplots(dpi=300, figsize=(5, 2))
    if  req_size == 1460:
        color = 'tab:orange'
        marker = 's'
    elif req_size == 1460000:
        color = 'tab:blue'
        marker = 'o'

    client_num_NS = np.array(data["client_number_NS"])     
    mean_throughput_NS = np.array(data["throughput_mean_NS"])
    ## sort the data based on the client number
    client_num_NS, mean_throughput_NS = sort_x_based_on_y(client_num_NS, mean_throughput_NS)
    
    
    client_num_baseline = np.array(data["client_number_baseline"])
    mean_throughput_baseline = np.array(data["throughput_mean_baseline"])
    ## sort the data based on the client number
    client_num_baseline, mean_throughput_baseline = sort_x_based_on_y(client_num_baseline, mean_throughput_baseline)    
        
    ax.loglog(client_num_NS, mean_throughput_NS, label=f"{req_size/1e3} KB (NS)", markersize=8, marker=marker, color=color)
    ax.loglog(client_num_baseline, mean_throughput_baseline, label=f"{req_size/1e3} KB (Baseline)", color=color, linestyle='--')



    plt.gca().set_xscale("log", base=2)
    # ax.legend(framealpha=0, handlelength=1, fontsize=12, ncol=1)
    plt.xticks([2**i for i in range(0,11, 2)])
    plt.yticks([10**i for i in range(2, 7, 2)])
    plt.legend(framealpha=0, handlelength=1, fontsize=12, ncol=1)
    ax.set_xlabel('Number of clients')
    ax.set_ylabel('Throughput (Reqs/s)')
    ax.set_title('Throughput vs Number of Clients')
    

    plot_file = os.path.join(results_dir, "throughput_vs_client_num.pdf")

    plt.savefig(plot_file, bbox_inches='tight', transparent=True)
    plt.close()
         

def plot_latency_vs_client_num(data, results_dir):
    req_size = np.unique(data["req_size_NS"])
    fig, ax = plt.subplots(dpi=300, figsize=(5, 2))
    if req_size == 1460:
        color = 'tab:orange'
        marker = 's'
    elif req_size == 1460000:
        color = 'tab:blue'
        marker = 'o'


    client_num_NS = np.array(data["client_number_NS"])     
    mean_latency_NS = np.array(data["latency_mean_NS"])
    ## sort the data based on the client number
    client_num_NS, mean_latency_NS = sort_x_based_on_y(client_num_NS, mean_latency_NS)

    
    client_num_baseline = np.array(data["client_number_baseline"])
    mean_latency_baseline = np.array(data["latency_mean_baseline"])
    ## sort the data based on the client number 
    client_num_baseline, mean_latency_baseline = sort_x_based_on_y(client_num_baseline, mean_latency_baseline)
     
        
    ax.loglog(client_num_NS, mean_latency_NS/1e3, label=f"{req_size/1e3} KB (NS)", markersize=8, marker=marker, color=color)
    ax.loglog(client_num_baseline, mean_latency_baseline/1e3, label=f"{req_size/1e3} KB (Baseline)", color=color, linestyle='--')
    

    plt.gca().set_xscale("log", base=2)
    # ax.legend(framealpha=0, handlelength=1, fontsize=12, ncol=1)
    plt.xticks([2**i for i in range(0,11, 2)])
    plt.yticks([10**i for i in range(-1, 5)])
    plt.legend(framealpha=0, handlelength=1, fontsize=12, ncol=1)   
    ax.set_xlabel('Number of clients')
    ax.set_ylabel('Latency (ms)')
    ax.set_title('Latency vs Number of Clients')
    plot_file = os.path.join(results_dir, "latency_vs_client_num.pdf")
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
        
    # results_dir = "/home/minesvpn/workspace/artifact_evaluation/code/minesvpn/evaluation/microbenchmarks/results/microbenchmark_(2024-03-18_12-22)"


    data_dir = os.path.join(results_dir, "processed_data.pkl")
    
    # Load the processed data
    with open(data_dir, "rb") as file:
        processed_data = pickle.load(file)
    
    print(processed_data)  
    # plot_latency_vs_dp_interval(processed_data, results_dir)    
    plot_throughput_vs_client_num(processed_data, results_dir)
    plot_latency_vs_client_num(processed_data, results_dir)


if __name__=="__main__":
    main()