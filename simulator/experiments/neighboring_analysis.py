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

def neighboring_analysis(config:configlib, df):
    df = data_filter_deterministic(config, df)
    print(df.shape)
    # Get the distance between queue states for average of videos
    # df_mean = df.groupby('label').mean()
    # print(df_mean.shape)
    # arr_mean = convert_df_to_array(df_mean)
    # # m, v, k = queue_mutual_distance_stats(arr_mean)
    # # sigma = np.sqrt(v / (k - 1))
    # # print("m: ", m)
    # print("sigma: ", sigma)

    # print("shape of dataframe", df.shape)
    # arr = np.reshape(convert_df_to_array(df), (-1)) 
    # print("max queue size", np.max(arr)/1e3)
    # print("min queue size", np.min(arr)/1e3)

    # index = np.where(arr <10e3)
    # print(index)
    # y = np.delete(arr, index) 
    # print()
    # print(np.min(y))
    # queue_values = np.reshape(y, (-1)) 

    # plt.figure()
    # plt.hist(queue_values/1e3, bins=100, density=True) 
    # plt.xlabel("Queue Size (in KB)")
    # plt.savefig("results/queue_size_5e5.pdf")

    # Get the distribution of distances between queue states for average of videos
    # dists = np.array(queue_mutual_distance_aligned(arr_mean)) 
    # # Plot the histogram of the distances as a distribution

    # print(np.min(dists))
    # plt.figure()
    # plt.hist(dists/1e3, bins=100, density=True)
    # plt.xlabel("Distance between two queue states (in KB)")
    # plt.savefig("results/queue_mutual_distance_5e5.pdf")
    # print("average queue distance (in KB)", np.mean(dists)/1e3) 


    df_average = df.sum(axis=1).mean()
    
    print(df_average/1e6)
    
 



    plt.figure()
    # For every video, get all distances from all other videos 
    arr_all = convert_df_to_array(df)
    print(arr_all.shape)
    print("max queue size", max_queue_size(arr_all))
    dists_traces = []
    with tqdm(total=arr_all.shape[0], desc="Calculating Trace and Queue Distances: ") as pbar:
        for i in range(arr_all.shape[0]):
            tmp_df = df[df['label'] != df.iloc[i]['label']] 
            tmp_arr = convert_df_to_array(tmp_df)
            dists_traces.append(get_dist_from_all_traces(tmp_arr, arr_all[i]))
            get_distance_from_all_queues_aligned(tmp_arr, arr_all[i])
            pbar.update(1)
    dists_traces = np.reshape(np.array(dists_traces), (-1))
    # Get the distribution of distances between videos for all videos
    plt.hist(dists_traces/1e6, bins=100, density=True)
    print("average video distance (in MB)", np.mean(dists_traces)/1e6)
    print("median video distance (in MB)", np.median(dists_traces)/1e6)
    print("25th percentile video distance (in MB)", np.percentile(dists_traces, 25)/1e6)
    print("75th percentile video distance (in MB)", np.percentile(dists_traces, 75)/1e6)
    print("max video distance (in MB)", np.max(dists_traces)/1e6)
    print("95th percentile video distance (in MB)", np.percentile(dists_traces, 95)/1e6)
    plt.xlabel("Distance between two video trace (in MB)")
    plt.savefig("results/video_mutual_distance_1e6.pdf")
    
    


    # plt.figure()
    # plt.hist(dists_queues/1e3, bins=100, density=True)
    # # plt.xlabel("Distance between two queue states (in KB)")
    # # plt.savefig("results/queue_mutual_distance_1e6.pdf")

    print("average queue distance (in KB)", (mean_form_dict(queue_distances))/1e3) 
    print("median queue distance (in KB)", percentile_from_dict(queue_distances, 0.5)/1e3) 
    print("25th percentile queue distance (in KB)", percentile_from_dict(queue_distances, 0.25)/1e3)
    print("75th percentile queue distance (in KB)", percentile_from_dict(queue_distances, 0.75)/1e3)
    print("85 percentile queue distance (in KB)", percentile_from_dict(queue_distances, 0.85)/1e3)
    print("90th percentile queue distance (in KB)", percentile_from_dict(queue_distances, 0.9)/1e3)
    # # plot the box plot of queue distances 
    # plt.figure()
    # plt.boxplot(dists_queues/1e3)
    # plt.savefig("results/queue_mutual_distance_boxplot_1e6.pdf")
    # return {}, {}
