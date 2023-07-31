import numpy as np
from tqdm import tqdm 
import configlib
import matplotlib.pyplot as plt

from src.data_utils.data_loader import data_filter_deterministic 


def get_distance_from_all_elemnts_stats(element, arr, prev_m, v, k):
    dist = np.reshape(abs(arr - element), (-1))
    for elem in np.nditer(dist):
        curr_m = prev_m + (elem - prev_m) / k
        v = v + (elem - prev_m) * (elem - curr_m)
        prev_m = curr_m
        k = k + 1
    return curr_m, v, k

def get_distance_from_all_elemnts(element, arr):
    dist = np.reshape(abs(arr - element), (-1))
    return dist

def queue_mutual_distance_stats(arr):
    ## We assume the arr is 2D numpy array
    # iterate over all elements and calculate the distance from all other elements
    dist_arr = []
    m  = v = 0
    k = 1 
    with tqdm(total=arr.size, desc="Calculating Mutual Distance: ") as pbar:
        for elem in np.nditer(arr):
            m, v, k = get_distance_from_all_elemnts_stats(elem, arr, m, v, k)   
            # print("m: ", m)
            # print("v: ", v)
            # print("k: ", k)     
            pbar.update(1)
    return m, v, k

def queue_mutual_distance(arr):
    dists = []
    with tqdm(total=arr.size, desc="Calculating Mutual Distance: ") as pbar:
        for elem in np.nditer(arr):
            dist = get_distance_from_all_elemnts(elem, arr)
            dists += dist.tolist()
            pbar.update(1)
    return dists


queue_distances = {}
def get_distance_from_all_queues_aligned(arr, vector):
    dist = abs(arr - vector)
    dist = np.reshape(dist, (-1))
    for element in dist:
        if element in queue_distances.keys():
            queue_distances[element] += 1
        else:
            queue_distances[element] = 1


def convert_df_to_array(df):
    #df.drop(columns=['label'], inplace=True)
    return df.to_numpy()

def get_dist_from_all_traces(arr, vector):
    dists = []
    for i in range(arr.shape[0]):
        dist = np.linalg.norm(arr[i] - vector, ord=1)
        dists.append(dist)
    # print(len(dists))
    return dists



def max_queue_size(arr):
    return np.max(arr)


def percentile_from_dict(data, percentile):
    """
    Calculate the given percentile from a dictionary that maps values to their frequency.
    """
    sorted_items = sorted(data.items())
    total_count = sum(data.values())
    target_count = int(percentile * total_count)

    cumulative_count = 0
    for value, count in sorted_items:
        cumulative_count += count
        if cumulative_count >= target_count:
            return value

    return sorted_items[-1][0]  # Return the maximum value if percentile is greater than 1

def mean_form_dict(data):
    total_sum = 0
    total_count = 0
    for key, value in data.items():
        total_sum += key * value
        total_count += value

    # Calculate the mean
    
    mean = total_sum / total_count
    return mean


def max_from_dict(data):
    return max(data.keys())

def min_from_dict(data):
    return min(data.keys())

def cdf_from_dict(data):
    """
    Calculate the CDF from a dictionary that maps values to their frequency.
    """
    sorted_items = sorted(data.items())
    print(sorted_items[0])
    total_count = sum(data.values())
    cumulative_count = 0
    cdf = []
    for value, count in sorted_items:
        cumulative_count += count
        cdf.append((value, cumulative_count / total_count))

    return cdf


def neighboring_analysis(config:configlib, df):
    df = data_filter_deterministic(config, df)
 



    plt.figure()
    # For every trace, get all distances from all other videos 
    arr_all = convert_df_to_array(df)
    traces_sizes = np.sum(arr_all, axis=1)
    print("max trace size (in KB)", max(traces_sizes)/1e3)
 
    dists_traces = []
    with tqdm(total=arr_all.shape[0], desc="Calculating Trace and Queue Distances: ") as pbar:
        for i in range(arr_all.shape[0]):
            tmp_df = df[df['label'] != df.iloc[i]['label']] 
            tmp_arr = convert_df_to_array(tmp_df)
            dists_traces.append(get_dist_from_all_traces(tmp_arr, arr_all[i]))
            get_distance_from_all_queues_aligned(tmp_arr, arr_all[i])
            pbar.update(1)
    dists_traces = np.reshape(np.array(dists_traces), (-1))
    results = {}
    
    dists_traces_MB = dists_traces/1e6
    results['trace_distance_min'] = np.min(dists_traces_MB)
    results['trace_distance_max'] = np.max(dists_traces_MB)
    results['trace_distance_25th'] = np.percentile(dists_traces_MB, 25)
    results['trace_distance_50th'] = np.percentile(dists_traces_MB, 50)
    results['trace_distance_75th'] = np.percentile(dists_traces_MB, 75)
    results['trace_distance_mean'] = np.mean(dists_traces_MB) 
    results['DP_interval_us'] =  config.data_time_resolution_us  
    counts, bin_edges = np.histogram(dists_traces_MB, bins=len(dists_traces_MB), density=True)
    results['traces_bin_edges'] = bin_edges.tolist()
    results['trace_distance_cdf'] = np.cumsum(counts)

    
    print("average trace distance (in MB)", np.mean(dists_traces)/1e6)
    print("median trace distance (in MB)", np.median(dists_traces)/1e6)
    print("25th percentile trace distance (in MB)", np.percentile(dists_traces, 25)/1e6)
    print("75th percentile trace distance (in MB)", np.percentile(dists_traces, 75)/1e6)
    print("85th percentile trace distance (in MB)", np.percentile(dists_traces, 85)/1e6)
    print("90th percentile trace distance (in MB)", np.percentile(dists_traces, 90)/1e6)
    print("95th percentile trace distance (in MB)", np.percentile(dists_traces, 95)/1e6)
    
    

    results['queue_distance_min'] = min_from_dict(queue_distances)
    results['queue_distance_max'] = max_from_dict(queue_distances)
    results['queue_distance_25th'] = percentile_from_dict(queue_distances, 0.25)
    results['queue_distance_50th'] = percentile_from_dict(queue_distances, 0.5)
    results['queue_distance_75th'] = percentile_from_dict(queue_distances, 0.75)
    results['queue_distance_cdf'] = cdf_from_dict(queue_distances)
    results['queue_distance_mean'] = mean_form_dict(queue_distances)

    print("average queue distance (in KB)", (mean_form_dict(queue_distances))/1e3) 
    print("median queue distance (in KB)", percentile_from_dict(queue_distances, 0.5)/1e3) 
    print("25th percentile queue distance (in KB)", percentile_from_dict(queue_distances, 0.25)/1e3)
    print("75th percentile queue distance (in KB)", percentile_from_dict(queue_distances, 0.75)/1e3)
    print("85th percentile queue distance (in KB)", percentile_from_dict(queue_distances, 0.85)/1e3)
    print("90th percentile queue distance (in KB)", percentile_from_dict(queue_distances, 0.9)/1e3)
    print("95th percentile queue distance (in KB)", percentile_from_dict(queue_distances, 0.95)/1e3)
    print("max queue distance (in KB)", max_from_dict(queue_distances)/1e3)
    # # plot the box plot of queue distances 
    # plt.figure()
    # plt.boxplot(dists_queues/1e3)
    # plt.savefig("results/queue_mutual_distance_boxplot_1e6.pdf")
    return {}, results
