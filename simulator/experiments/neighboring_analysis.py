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


def neighboring_analysis(config:configlib, df):
    df = data_filter_deterministic(config, df)
    # Get the distance between queue states for average of videos
    # df_mean = df.groupby('label').mean()
    # print(df_mean.shape)
    # arr_mean = convert_df_to_array(df_mean)
    # m, v, k = queue_mutual_distance_stats(arr_mean)
    # sigma = np.sqrt(v / (k - 1))
    # print("m: ", m)
    # print("sigma: ", sigma)

    print("shape of dataframe", df.shape)
    arr = np.reshape(convert_df_to_array(df), (-1)) 
    print("max queue size", np.max(arr)/1e3)
    print("min queue size", np.min(arr)/1e3)

    index = np.where(arr <10e3)
    print(index)
    y = np.delete(arr, index) 
    print()
    print(np.min(y))
    queue_values = np.reshape(y, (-1)) 

    plt.figure()
    plt.hist(queue_values/1e3, bins=100, density=True) 
    plt.xlabel("Queue Size (in KB)")
    plt.savefig("results/queue_size_5e4.pdf")
    
    # # Get the distribution of distances between queue states for average of videos
    # dists = np.array(queue_mutual_distance(arr_mean)) 
    # # Plot the histogram of the distances as a distribution
    # dists = np.delete(dists, np.where(dists == 0))
    # print(np.min(dists))
    # plt.figure()
    # plt.hist(dists/1e3, bins=100, density=True)
    # plt.xlabel("Distance between two queue states (in KB)")
    # plt.savefig("results/queue_mutual_distance.pdf")


    # plt.figure()
    # # For every video, get all distances from all other videos 
    # arr_all = convert_df_to_array(df)
    # print(arr_all.shape)
    # print("max queue size", max_queue_size(arr_all))
    # dists = []
    # with tqdm(total=arr_all.shape[0], desc="Calculating Trace Distances: ") as pbar:
    #     for i in range(arr_all.shape[0]):
    #         tmp_df = df[df['label'] != df.iloc[i]['label']] 
    #         tmp_arr = convert_df_to_array(tmp_df)
    #         dists.append(get_dist_from_all_traces(tmp_arr, arr_all[i]))
    #         pbar.update(1)
    # dists = np.reshape(np.array(dists), (-1))
    # print("dists min", np.min(dists))
    # print("dists max", np.max(dists))
    
    # # Get the distribution of distances between videos for all videos
    # plt.hist(dists/1e6, bins=100, density=True)
    # plt.xlabel("Distance between two video trace (in MB)")
    # plt.savefig("results/video_mutual_distance.pdf")

   
    
    
    return {}, {}
