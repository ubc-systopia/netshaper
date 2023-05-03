import pandas as pd
import os
import math
from tqdm import tqdm
from threading import Thread

class DataProcessorNew():
    def __init__(self, dir, num_of_classes, iter_num):
        self.directory = dir
        self.num_of_classes = num_of_classes
        self.iter_num = iter_num 
        self.ds_list = [None] * (self.num_of_classes * self.iter_num) 
        self.all_data_without_noise = 0
        self.all_data_with_noise = 0
    
    def get_video_names(self):
        first_iter_dir = os.path.join(self.directory, '1')
        video_names = os.listdir(first_iter_dir)
        return video_names
    
    def map_video_names_to_labels(self, video_names):
        video_label_mapping = {}
        for label in range(self.num_of_classes):
            video_label_mapping[str(label)] = video_names[label]
        return video_label_mapping

    def get_all_iters_for_a_label(self, label):
        video_name = self.map_video_names_to_labels(self.get_video_names())[str(label)]
        all_iters_for_label = []
        for iter in range(1, self.iter_num+1):
            iter_dir = os.path.join(self.directory, str(iter))
            video_dir = os.path.join(iter_dir, video_name)
            # Check weather video_dir is a directory or a file
            if(os.path.isdir(video_dir)):
                video_file = os.listdir(video_dir)[0]
            else:   
                video_file = video_dir
            all_iters_for_label.append(os.path.join(iter_dir, video_name, video_file))
        return all_iters_for_label
    
    def get_label_filename_mapping(self):
        label_file_mapping = {}
        for label in range(self.num_of_classes):
            label_file_mapping[str(label)] = self.get_all_iters_for_a_label(label)
        return label_file_mapping 

    def get_all_iters_for_videos(self, video_names):
        label_file_mapping = {}
        for label in range(self.num_of_classes):
            for iter in range(1, self.iter_num):
                iter_dir = os.path.join(self.directory, str(iter))
                video_dir = os.path.join(iter_dir, video_names[label])
                video_file = os.listdir(video_dir)[0]
                label_file_mapping[str(label)] = os.path.join(iter_dir, video_names[label])
        return label_file_mapping

    # def map_filename_to_label(self, label):
    #     files_for_label = []
    #     dir_of_label = os.path.join(self.directory, str(label+1))
        
    #     # Getting list of all directories in the directory of the label
    #     all_dirs_for_label = os.listdir(dir_of_label)
    #     for dir in all_dirs_for_label:
    #         # Getting list off all files in the directory of the label
    #         file_in_dir = os.listdir(os.path.join(dir_of_label, dir))
            
    #         # Adding the file name to the list of files for the label
    #         files_for_label.append(os.path.join(dir_of_label, dir, file_in_dir[0]))
    #     return files_for_label
    
    # def get_label_filename_mapping(self):
    #     label_file_mapping = {}
    #     for label in range(self.num_of_classes):
    #         label_file_mapping[str(label)] = self.map_filename_to_label(label)
    #     return label_file_mapping
            
    def get_dataframe(self, dataset_name):
        colnames = ['pkt_time', 'pkt_size']
        df = pd.read_csv(dataset_name, delimiter = "\t", names = colnames, header=None)
        df.fillna(0, inplace=True)
        remove_request = True
        if remove_request:
            for i in range(len(df)):
                # print(df.iloc[i, 1])
                if (df.iloc[i, 1] != 0): 
                    break
            df = df.drop(range(i))    
        # print(df.head)
        return df
    
    def burst_in_window_pattern_simple(self, df, window_size):
    
        df['pkt_time']  = df['pkt_time'] * 1e6

        pkt_time_column_indx = df.columns.get_loc('pkt_time')
        pkt_size_column_indx = df.columns.get_loc('pkt_size')

        # Sort the dataframe based on packet time
        df = df.sort_values(by=['pkt_time'])      

        stream_start_time = df.iloc[0, pkt_time_column_indx]
        stream_end_time = df.iloc[-1, pkt_time_column_indx]
        window_num = int(math.ceil((stream_end_time - stream_start_time)/window_size)) 

        burst_size_per_window = [0] * window_num
        burst_time_per_window = [(elem + 1/2) * window_size for elem in range(window_num)]
        for i in range(len(df)): 
            # print(math.floor((df.iloc[i, pkt_time_column_indx] - stream_start_time)/window_size))
            # print("current packet time: ", df.iloc[i, pkt_time_column_indx]) 
            # print("start time: ", stream_start_time)
            burst_size_per_window[math.floor((df.iloc[i, pkt_time_column_indx] - stream_start_time - 1)/window_size)] += df.iloc[i, pkt_size_column_indx]
        
        data = {'window time': burst_time_per_window, 'window size': burst_size_per_window}
        windowed_df = pd.DataFrame(data=data)
        return windowed_df   
    
   
    def process_data(self, ds, key, time_resolution_us, index):
        df = self.get_dataframe(ds)
        
        windowed_df = self.burst_in_window_pattern_simple(df, time_resolution_us).drop(columns=['window time'])
        sample = (windowed_df.T).reset_index()
        sample['label'] = int(key)
        self.ds_list[index] = sample
  



    # def aggregated_dataframe(self, time_resolution_us):
    #     print(time_resolution_us)
    #     ds_name_dict = self.get_label_filename_mapping()
    #     keys = list(ds_name_dict.keys())
    #     aggregated_df = pd.DataFrame()
    #     ds_num = len(keys)*len(ds_name_dict[keys[0]])
    #     with tqdm(total=ds_num, desc="Processing Data: ") as pbar:
    #         threads = []
    #         index = 0
    #         for key in keys:
    #             for ds in ds_name_dict[key]:
    #                 thread = Thread(target=self.process_data, args=(ds, key, time_resolution_us, index))
    #                 threads.append(thread)
    #                 thread.start()
    #                 index += 1
    #         for thread in threads:
    #             thread.join()
    #             pbar.update(1)
    #     aggregated_df = pd.concat(self.ds_list, ignore_index=True) 
    #     aggregated_df.drop(columns=['index'], inplace=True)
    #     aggregated_df.fillna(0, inplace=True)
    #     ## Move the label column to the end
    #     column_to_move = aggregated_df.pop('label')
    #     aggregated_df.insert(len(aggregated_df.columns), 'label', column_to_move)
    #     return aggregated_df.astype(int)
            
    def aggregated_size (self, df): 
        self.all_data_size += df['pkt_size'].sum()
        
    def aggregated_dataframe(self, time_resolution_us):
        print(time_resolution_us)
        ds_name_dict = self.get_label_filename_mapping()
        flag = True
        keys = list(ds_name_dict.keys())
        with tqdm(total=len(keys) * len(ds_name_dict[keys[0]]), desc="Processing Data: ") as pbar:
            for key in keys:
                for ds in ds_name_dict[key]:
                    df = self.get_dataframe(ds)
                    windowed_df = self.burst_in_window_pattern_simple(df, time_resolution_us).drop(columns=['window time'])
                    sample = (windowed_df.T).reset_index()
                    sample['label'] = int(key)
                    sample['video_name'] = os.path.basename(ds)
                    if flag:
                        aggregated_df = sample
                        flag = False   
                    else:
                        aggregated_df = pd.concat([aggregated_df, sample], ignore_index=True) 
                    pbar.update(1)
        aggregated_df.drop(columns=['index'], inplace=True)
        aggregated_df.fillna(0, inplace=True)
        ## Move the label column to the end
        column_to_move = aggregated_df.pop('label')
        # Remove columns from the end of the dataframe that only contain zeros
        drop_indices = []
        for i in range(len(aggregated_df.columns)-1, 0, -1):
            if aggregated_df.iloc[:,i].sum() == 0:
                    drop_indices.append(i)
            else:
                break
        aggregated_df.drop(aggregated_df.columns[drop_indices], axis=1, inplace=True)
        aggregated_df.insert(len(aggregated_df.columns), 'label', column_to_move)
        column_to_move = aggregated_df.pop('video_name')
        aggregated_df.insert(len(aggregated_df.columns), 'video_name', column_to_move)
        return aggregated_df



    def test(self):
        ad = self.aggregated_dataframe(1e6)
        # ds_name_dict = self.get_label_filename_mapping()
        # for video_title in ds_name_dict:
        #     for trace in ds_name_dict[video_title]:
        #         df = self.get_dataframe(trace)
        #         print(df.tail(10))
        #         break        
        print(ad.head(10))
        
    
        
if __name__ == "__main__":
    dp = DataProcessorNew("/home/ubuntu/data/st-v10-t10-T50000-s38/Peer1", 10, 10)
    dp.test()
        
