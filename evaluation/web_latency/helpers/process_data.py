import argparse
import pickle
import re
import os



def get_interval_list(results_dir):
    # Getting the list of all direcotries in the results directory
    dirs = [d for d in os.listdir(results_dir) if os.path.isdir(os.path.join(results_dir, d))]

    # Removing T_ from the beginning of the directory names
    intervals = [int(d.replace("T_", "")) for d in dirs]
    
    return intervals  



def get_interval_dir(results_dir, interval):
    return os.path.join(results_dir, "T_" + str(interval), "client")


def get_log_file(interval_dir):
    # Get the file that is named wrk.log and is in one of the subdirectories of the interval directory
    for root, dirs, files in os.walk(interval_dir):
        for file in files:
            if file.endswith("wrk.log"):
                return os.path.join(root, file)
    return None


def get_latency_stats(log_file):
    mean_value = None
    std_dev = None

    # Open the file
    with open(log_file, 'r') as file:
        # Read each line
        for line in file:
            # Search for lines containing "Mean" and "StdDeviation"
            if "Mean" in line:
                # Extract mean value using regular expression
                mean_match = re.search(r'Mean\s*=\s*([\d.]+)', line)
                if mean_match:
                    mean_value = float(mean_match.group(1))
            if "StdDeviation" in line:
                # Extract standard deviation value using regular expression
                std_dev_match = re.search(r'StdDeviation\s*=\s*([\d.]+)', line)
                if std_dev_match:
                    std_dev = float(std_dev_match.group(1))
            
            # Break loop if both mean and standard deviation are found
            if mean_value is not None and std_dev is not None:
                break 
    return mean_value, std_dev




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
    results_dir = "/home/minesvpn/workspace/artifact_evaluation/code/minesvpn/evaluation/web_latency/results/web_latency_(2024-02-07_13-17)"
    
    
    
    processed_data = {"dp_intevals": [], "mean": [], "std": []}
    
    dp_intervals = get_interval_list(results_dir)      

    
    for interval in dp_intervals:
        # Get the directory for the interval
        interval_dir = get_interval_dir(results_dir, interval)

        # Get the list of all log files in the interval directory
        log_file = get_log_file(interval_dir)
        
        if log_file is None:
            raise Exception(f'No log file found in {interval_dir}')
        
        mean, std = get_latency_stats(log_file) 
        
        processed_data["dp_intevals"].append(interval)
        processed_data["mean"].append(mean)
        processed_data["std"].append(std)
    
    
    # Saving the processed data in the same results directory as a pickle file
    with open(os.path.join(results_dir, "processed_data.pkl"), "wb") as file:
        pickle.dump(processed_data, file)
    
    print(f'Processed data saved in {results_dir}/processed_data.pkl')


if __name__ == "__main__":
    main()