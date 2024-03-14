import argparse
import pickle
import re
import os
import pandas as pd
import numpy as np

def get_client_num_list(results_dir):
    # Getting the list of all direcotries in the results directory
    dirs = [d for d in os.listdir(results_dir) if os.path.isdir(os.path.join(results_dir, d))]

    # Removing T_ from the beginning of the directory names
    client_numbers = [int(d.replace("C_", "")) for d in dirs]
    
    return client_numbers

def get_client_num_dir(results_dir, client_num):
    return os.path.join(results_dir, "C_" + str(client_num), "client")


def get_log_files(interval_dir):
    # Get the file that is named wrk.log and is in one of the subdirectories of the interval directory
    log_files = []
    for root, dirs, files in os.walk(interval_dir):
        for file in files:
            if file.endswith("wrk.log"):
                log_files.append(os.path.join(root, file))
    return log_files



def get_csv_files(interval_dir):
    # Get the file that is named wrk.log and is in one of the subdirectories of the interval directory
    csv_files = []
    for root, dirs, files in os.walk(interval_dir):
        for file in files:
            if file.endswith("stats.csv"):
                csv_files.append(os.path.join(root, file))

    return csv_files

def get_throughput_stats(log_files, client_num):
    throughput = []
    for log_file in log_files:
        pattern = r'(\d+) requests'
        # Initialize variable to store the number of requests
        num_requests = None
        # Open the file and read its content
        with open(log_file, 'r') as file:
            content = file.read()

            # Find the line containing the requests count
            match = re.search(pattern, content)
            if match:
            # Extract the number of requests
                num_requests = int(match.group(1))    

        if num_requests is None:
            raise Exception(f'No requests count found in {log_file}')
        throughput.append(num_requests)
    return np.mean(throughput), np.std(throughput)



def get_latency_stats(csv_files, client_num):
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
    return np.mean(latencies), np.std(latencies)

 
def get_request_size(log_file):
    # Extracting the number after "req_"
    req_number = None
    match = re.search(r'req_(\d+)', log_file)
    if match:
        req_number = int(match.group(1))

    return req_number
   
  



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
    # TODO: Remove this
    # results_dir = "/home/minesvpn/workspace/artifact_evaluation/code/minesvpn/evaluation/microbenchmarks/results/microbenchmark_(2024-03-14_00-57)"
    
    
    
    processed_data = {"client_number": [], "latency_mean": [], "latency_std": [], "throughput_mean":[], "throughput_std": [], "req_size": []}
    
    client_numbers = get_client_num_list(results_dir)        
    
    for client_number in client_numbers:
        # Get the directory for the client number
        client_dir = get_client_num_dir(results_dir, client_number)

        # print(client_number, client_dir) 
        # Get csv files in the client directory
        csv_files = get_csv_files(client_dir)
        
        if len(csv_files) == 0:
            raise Exception(f'No csv file found in {client_dir}')
        
        latency_mean, latency_std = get_latency_stats(csv_files, client_number)        
                
        # Get the log file in the client directory
        log_files = get_log_files(client_dir)

        throughput_mean, throughput_std = get_throughput_stats(log_files, client_number)
        
        
        # return
        # Get the request size from the log file
        req_size = get_request_size(log_files[0])
        
        
        processed_data["client_number"].append(client_number)
        processed_data["req_size"].append(req_size)
        processed_data["latency_mean"].append(latency_mean)
        processed_data["latency_std"].append(latency_std)
        processed_data["throughput_mean"].append(throughput_mean)
        processed_data["throughput_std"].append(throughput_std)


        
    # print(processed_data)
    # Saving the processed data in the same results directory as a pickle file
    with open(os.path.join(results_dir, "processed_data.pkl"), "wb") as file:
        pickle.dump(processed_data, file)
    
    print(f'Processed data saved in {results_dir}/processed_data.pkl')



if __name__ == "__main__":
    main()