import torch
import os
import pandas as pd
import torch.optim as optim
from torchvision import datasets, transforms
from torch.autograd import Variable
from typing import Any, Tuple
import torch.nn.functional as F
from torch.utils.data import Dataset, DataLoader, random_split
from TCN.TCN.mnist_pixel.model import TCN
import configlib
import numpy as np
from sklearn.metrics import recall_score, precision_score

class TrafficTraces(Dataset):
    def __init__(self,
                 data_df: pd.DataFrame,
                 n_classes: int,
                 normalize: bool = True) -> None:
        """
        Args:
            data_file (string): relative path to the data file starting from the data root directory
            root_dir (string): absolute path to the directory with all data files 
        """
        self.n_classes = n_classes
        self.normalize = normalize
        self.data_df = data_df

    def __len__(self):
        return len(self.data_df) 
    
    def __getitem__(self, index: int) -> Tuple[Any, Any]:
        """
        Args:
            index (int): Index

        Returns:
            Tuple[Any, Any]: tuple(trace, target) where the target is the index of the target class
        """
        trace = self.data_df.iloc[index, :-1]
        target = self.data_df.iloc[index, -1]
        if (self.normalize):
            trace = trace/trace.sum()    
        
        return torch.tensor(trace.values), (target)      
    
def data_transformer_traces(data_df: pd.DataFrame,
                          n_classes: int,
                          batch_size: int,
                          normalize: bool = True,
                          test_ratio: float = 0.2):

    traces_dataset = TrafficTraces(data_df,
                                   n_classes,
                                   normalize)    
    dataset_size = len(traces_dataset)
    train_size = int((1-test_ratio) * dataset_size)
    test_size = dataset_size - train_size
    train_set, test_set = random_split(traces_dataset, [train_size, test_size]) 

    train_loader = DataLoader(train_set, batch_size=batch_size)
    test_loader = DataLoader(test_set, batch_size=batch_size)
    return train_loader, test_loader

def train_test_and_report_acc(config: configlib.Config, df: pd.DataFrame, verbose=False):
    '''
    Default values 
    '''
    dropout = 0.05
    clip = -1
    kernel_size = 7
    levels = 8
    lr = 0.002 
    optim_t = "Adam"
    nhid = 25
    seed = 1111 
    input_channels = 1

    '''
    Configurable values 
    '''
    batch_size = config.attack_batch_size
    epoch_num = config.attack_epoch_num
    num_classes = config.num_of_unique_streams

    train_loader, test_loader = data_transformer_traces(df, num_classes, batch_size, normalize=False)
    
    channel_sizes = [nhid] * levels  
    model = TCN(input_channels, num_classes, channel_sizes, kernel_size=kernel_size, dropout=dropout) 
    model.cuda() 

    optimizer = getattr(optim, optim_t)(model.parameters(), lr=lr)
    
    def train(ep):
        train_loss = 0
        model.train()
        for batch_idx, (data, target) in enumerate(train_loader):
            data, target = data.cuda(), target.cuda()
            # data = data.view(-1, input_channels, seq_length)
            # if args.permute:
            #     data = data[:, :, permute]
            data = torch.unsqueeze(data, 1).float()
            data, target = Variable(data), Variable(target)
            optimizer.zero_grad()
            output = model(data)
            loss = F.nll_loss(output, target)
            loss.backward()
            if clip > 0:
                torch.nn.utils.clip_grad_norm_(model.parameters(), clip)
            optimizer.step()
            train_loss += loss
            # if batch_idx > 0 and batch_idx % args.log_interval == 0:
        if verbose:
            print('Train Epoch: {} [{}/{} ({:.0f}%)]\tLoss: {:.6f}'.format(
            ep, batch_idx * batch_size, len(train_loader.dataset),
            100. * batch_idx / len(train_loader), train_loss.item()))
        train_loss = 0
    
    
    def test():
        model.eval()
        test_loss = 0
        correct = 0
        all_preds = []
        all_targets = []
        with torch.no_grad():
            for data, target in test_loader:
                data, target = data.cuda(), target.cuda()
                data = torch.unsqueeze(data, 1).float()
                data, target = Variable(data), Variable(target)
                output = model(data)
                test_loss += F.nll_loss(output, target, reduction='sum').item()
                pred = output.data.max(1, keepdim=True)[1]
                correct += pred.eq(target.data.view_as(pred)).cpu().sum()
                all_preds.append(pred.cpu().numpy())
                all_targets.append(target.cpu().numpy())
                
            test_loss /= len(test_loader.dataset)
            test_acc = correct / len(test_loader.dataset)
            all_preds = np.concatenate(all_preds)
            all_targets = np.concatenate(all_targets)
            precision = precision_score(all_targets, all_preds, average='macro')
            recall = recall_score(all_targets, all_preds, average='macro')
            # precision = 0
            # recall = 0            
            if verbose:
                print('\nTest set: Average loss: {:.4f}, Accuracy: {}/{} ({:.0f}%)\n'.format(
                test_loss, correct, len(test_loader.dataset),
                100. * correct / len(test_loader.dataset)))
                print('Precision: {:.4f}, Recall: {:.4f}\n'.format(precision, recall))
            return test_acc, precision, recall

        
    
    for epoch in range(1, epoch_num+1):
       train(epoch)
       acc, prec, recal = test()
       if epoch % 10 == 0:
           lr /= 10
           for param_group in optimizer.param_groups:
               param_group['lr']

    model_checkpoint = {
        'model_state_dict': model.state_dict(),
        'optimizer_state_dict': optimizer.state_dict(),
        'epoch_num': epoch_num
    }

    return acc, prec, recal, model_checkpoint
        