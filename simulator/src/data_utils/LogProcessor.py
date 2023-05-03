import pandas as pd
import os
import math
from tqdm import tqdm


class LogProcessor():
    def __init__(self, dir, iter_num):
        self.directory = dir
        self.iter_num = iter_num
    
    
    def get_video_names(self):
        first_iter_dir = os.path.join(self.directory, '1')
        video_names = os.listdir(first_iter_dir)
        return video_names

    def get_all_iters_for_a_video(self, video_name):
        all_iters_for_video = []
        for iter in range(1, self.iter_num+1):
            iter_dir = os.path.join(self.directory, str(iter))
            video_dir = os.path.join(iter_dir, video_name)
            # Check weather video_dir is a directory or a file
            if(os.path.isdir(video_dir)):
                video_file = os.listdir(video_dir)[0]
            else:   
                video_file = video_dir
            all_iters_for_video.append(os.path.join(iter_dir, video_name, video_file))
        return all_iters_for_video


class DPDecisionProcessor(LogProcessor):
    def __init__(self, dir: str, iter_num: int):
        super().__init__(dir, iter_num) 

    def get_size_and_DP_decisions(self):
        DP_decisions = []
        aggregated_sizes = [] 
        video_names = self.get_video_names()
        for video in video_names:
            all_iters_for_video = self.get_all_iters_for_a_video(video)
            for iter in all_iters_for_video:
                with open(iter, 'r') as f:
                    for line in f:
                        if 'DPDecision' in line and 'aggregatedSize' in line:
                            # split the line by comma
                            elements = line.split(',')
                            # Take the value of the DPDecision after the "="
                            if(len(elements[0].split('=')) != 2 or len(elements[1].split('=')) != 2):
                                continue
                            DPDecision = elements[1].split('=')[1]
                            aggregatedSize = elements[0].split('=')[1]
                            # Check that DPDecision is a number
                            if (len(DPDecision.split(' ')) > 1):
                                continue
                            # print(DPDecision)
                            DP_decisions.append(int(DPDecision))
                            aggregated_sizes.append(int(aggregatedSize))
        
        return {"aggregated_sizes": aggregated_sizes, "DP_decisions": DP_decisions}


class RTTProcessor(LogProcessor):
    def __init__(self, dir, iter_num):
        super().__init__(dir, iter_num)

    def get_RTTs(self):
        RTTs = []
        video_names = self.get_video_names()
        for video in video_names:
            all_iters_for_video = self.get_all_iters_for_a_video(video)
            for iter in all_iters_for_video:
                with open(iter, 'r') as f:
                    for line in f:
                        if 'Error' not in line:
                            RTTs.append(int(line))
        return RTTs
    
    
