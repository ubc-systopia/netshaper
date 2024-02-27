import argparse
import os
import numpy as np
import pickle






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

def get_log_files(interval_dir):
    # Getting the list of all files in all subdirectories of the interval directory that end with .log
    log_files = []
    for root, dirs, files in os.walk(interval_dir):
        for file in files:
            if file.endswith(".log"):
                log_files.append(os.path.join(root, file)) 
    return log_files


def get_all_latencies(log_files):
    latencies = []
    for file in log_files:
        with open(file, "r") as f:
            lines = f.readlines()
            for line in lines:
                # Skip the line if it starts with Error
                if line.startswith("Error"):
                    continue
                else:
                    latencies.append(int(line))
    return latencies 

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
        
        
        
        
    processed_data = {"client_num": [], "dp_intevals": [], "mean": [], "std": []} 
    
    
    
    client_nums = get_clients_list(results_dir) 
    
    for client in client_nums:
        # Get the directory for the client    
        client_dir = get_client_dir(results_dir, client)
        
        dp_intervals = get_interval_list(client_dir)      

        for interval in dp_intervals:


            # Get the directory for the interval
            interval_dir = get_interval_dir(client_dir, interval)

            # Get the list of all log files in the interval directory
            log_files = get_log_files(interval_dir)

            # Get the latencies from the log files
            latencies = get_all_latencies(log_files)
            
            # Get the mean and variance of the latencies        
            mean, std = get_latency_stats(latencies)

            processed_data["dp_intevals"].append(interval)
            processed_data["mean"].append(mean)
            processed_data["std"].append(std)
            processed_data["client_num"].append(client)
                        
    print(processed_data)
    # Saving the processed data in the same results directory as a pickle file
    with open(os.path.join(results_dir, "processed_data.pkl"), "wb") as f:
        pickle.dump(processed_data, f)
    
    print(f'Processed data saved in {results_dir}/processed_data.pkl') 
            
if __name__ == "__main__":
    main()