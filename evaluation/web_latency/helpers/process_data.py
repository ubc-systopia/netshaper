import argparse
import pickle
import re
import os
import pandas as pd
import numpy as np

def get_interval_list(results_dir):
    # Getting the list of all direcotries in the results directory
    dirs = [d for d in os.listdir(results_dir) if os.path.isdir(os.path.join(results_dir, d))]

    # Removing T_ from the beginning of the directory names
    intervals = [int(d.replace("T_", "")) for d in dirs]
    
    return intervals  


def get_clients_list(results_dir):
    # Getting the list of all direcotries in the results directory
    dirs = [d for d in os.listdir(results_dir) if os.path.isdir(os.path.join(results_dir, d))]

    # Removing T_ from the beginning of the directory names
    clients = [int(d.replace("C_", "")) for d in dirs]
    
    return clients  


def get_interval_dir(results_dir, interval):
    return os.path.join(results_dir, "T_" + str(interval), "client")

def get_client_dir(results_dir, client):
    return os.path.join(results_dir, "C_" + str(client))



def get_csv_files(interval_dir):
    # Get the file that is named wrk.log and is in one of the subdirectories of the interval directory
    csv_files = []
    for root, dirs, files in os.walk(interval_dir):
        for file in files:
            if file.endswith("stats.csv"):
                csv_files.append(os.path.join(root, file))

    return csv_files


def get_latency_stats(latencies):
    mean = np.mean(latencies)
    std = np.std(latencies)
    return mean, std


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
        
        
        
        
    # Temporary reults dir for testing
    results_dir = "/home/minesvpn/workspace/artifact_evaluation/code/minesvpn/evaluation/web_latency/results/web_latency_(2024-02-28_16-34)"
    
    
    
    processed_data = {"client_num":[], "dp_intevals": [], "mean": [], "std": []}
    
    client_nums = get_clients_list(results_dir) 
    for client_num in client_nums:
        # Get the directory for the client    
        client_dir = get_client_dir(results_dir, client_num)
        
        dp_intervals = get_interval_list(client_dir)
        for dp_interval in dp_intervals:
            # Get the directory for the interval
            interval_dir = get_interval_dir(client_dir, dp_interval)
            # Get the list of all log files in the interval directory
            csv_files = get_csv_files(interval_dir)
            latencies = []
            for csv_file in csv_files: 
                # read the csv file as a pandas dataframe
                df = pd.read_csv(csv_file)
                df = df.rename(columns={'start_time 0': ' start_time 0'}) 
                for i in range(client_num):
                    columns_to_select = [f' start_time {i}', f' actual_time {i}']
                    # drop the tail
                    client_df = df.loc[:, columns_to_select] 
                    # drop row that conian string ' '
                    client_df = client_df[~((client_df == ' ').any(axis=1))]
                    client_df = client_df.dropna().astype(float)
                    T = 60 * 1e6
                    # Take values from actual time column with start time more than T 
                    client_df = client_df[client_df[f' start_time {i}'] > T]
                    # change the actual time values from string to int
                    latencies = latencies + list(client_df[f' actual_time {i}'] ) 
            # print(latencies)
            mean, std = get_latency_stats(latencies)

            processed_data["dp_intevals"].append(dp_interval)
            processed_data["client_num"].append(client_num)
            processed_data["mean"].append(mean)
            processed_data["std"].append(std)
   
    # print(processed_data)
    # Saving the processed data in the same results directory as a pickle file
    with open(os.path.join(results_dir, "processed_data.pkl"), "wb") as file:
        pickle.dump(processed_data, file)
    
    print(f'Processed data saved in {results_dir}/processed_data.pkl')


if __name__ == "__main__":
    main()