import torch
import numpy as np
import pandas as pd
import torch.nn as nn
from sklearn.model_selection import train_test_split
from torch.utils.data import DataLoader, Dataset
import torch.nn.functional as F
import torch.optim as optim
import configlib
from sklearn.metrics import recall_score, precision_score

class CustomDataset(Dataset):
    def __init__(self, data, labels):
        self.data = data
        self.labels = labels
    
    def __len__(self):
        return len(self.data)
    
    def __getitem__(self, index):
        x = self.data[index]
        y = self.labels[index]
        return x, y

class TCPDumpModel(torch.nn.Module):
    def __init__(self, num_classes, rows, cols, nb_filters, psize, ksize):
        super(TCPDumpModel, self).__init__()
        self.conv1 = torch.nn.Conv1d(cols, nb_filters, ksize)
        self.conv2 = torch.nn.Conv1d(nb_filters, nb_filters, ksize)
        self.conv3 = torch.nn.Conv1d(nb_filters, nb_filters, ksize)
        self.max_pool = torch.nn.MaxPool1d(psize)
        self.dropout1 = torch.nn.Dropout(p=0.3)
        self.dropout2 = torch.nn.Dropout(p=0.2)
        self.dropout3 = torch.nn.Dropout(p=0.2)
        self.flatten = torch.nn.Flatten()
        # print(rows)
        # print("dense1 input dim", (nb_filters*(rows - (ksize - 1)*3) // psize))
        self.dense1 = torch.nn.Linear(nb_filters*((rows - (ksize - 1)*3) // psize), 64)
        self.dense2 = torch.nn.Linear(64, num_classes)

    def forward(self, x):
        print_shape = False
        x = torch.unsqueeze(x,1)
        if print_shape: print("input", x.shape)
        x = self.conv1(x)
        if print_shape: print("conv1", x.shape)
        x = torch.nn.functional.relu(x)

        if print_shape: print(x.shape)
        x = self.conv2(x)
        if print_shape: print("conv2", x.shape)
        x = torch.nn.functional.relu(x)
        x = self.conv3(x)
        if print_shape: print("con3", x.shape)
        x = torch.nn.functional.relu(x)
        x = self.dropout1(x)
        if print_shape: print("dropout1", x.shape)
        x = self.max_pool(x)
        if print_shape: print("max_pool", x.shape)
        x = self.dropout2(x)
        if print_shape: print("dropout2", x.shape)
        x = self.flatten(x)
        if print_shape: print("flatten", x.shape)
        x = self.dense1(x)
        if print_shape: print("dense1", x.shape)
        x = torch.nn.functional.relu(x)
        x = self.dropout3(x)
        if print_shape: print("dropout3", x.shape)
        x = self.dense2(x)
        if print_shape: print(x.shape)
        x = torch.nn.functional.softmax(x, dim=1)
        if print_shape: print(x.shape)
        return x



def train_test_model(model, train_loader, test_loader, optimizer, criterion, num_epochs):
    for epoch in range(num_epochs):
        running_loss = 0.0
        running_corrects = 0
        
        model.train()
        for inputs, labels in train_loader:
            optimizer.zero_grad()
            
            outputs = model(inputs.float())
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            
            pred_int = outputs.argmax(dim=1, keepdim=True) # get the index of the max log-probability
            labels_int = labels.argmax(dim=1, keepdim=True)
            running_corrects += (pred_int == labels_int).sum().item() 
            
    
            
        epoch_loss = running_loss / len(train_loader.dataset)
        epoch_acc = running_corrects / len(train_loader.dataset)

    model.eval() # switch to evaluation mode
    with torch.no_grad():
        test_loss = 0
        test_acc = 0
        all_preds = []
        all_targets = []
        for data, target in test_loader:
            output = model(data.float())
            test_loss += F.cross_entropy(output, target, reduction='sum').item()
            pred_int = output.argmax(dim=1, keepdim=True) # get the index of the max log-probability
            labels_int = target.argmax(dim=1, keepdim=True)
            test_acc += (pred_int == labels_int).sum().item()
            all_preds += pred_int.flatten().tolist()
            all_targets += labels_int.flatten().tolist()
        test_loss /= len(test_loader.dataset)
        test_acc /= len(test_loader.dataset)
        precision = precision_score(all_targets, all_preds, average='macro')
        recall = recall_score(all_targets, all_preds, average='macro')
    return test_acc, precision, recall
    

            
      

def train_test_and_report_acc(config: configlib.Config, df):
    epoch_num = config.attack_epoch_num
    batch_size = config.attack_batch_size
    num_classes = config.num_of_unique_streams
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    
    # Dataset Normalization 
    x = df.iloc[:, :-1].values
    y = df.iloc[:, -1].values
    norm = np.sqrt(np.square(x).sum(axis=1))
    x = np.divide(x, np.vstack([norm]*x.shape[1]).T)
    
    # One hot encoding and splitting dataset to test and train
    one_hot_encoded_y = np.eye(num_classes)[y]
    train_X, test_X, train_label, test_label = train_test_split(x, one_hot_encoded_y, test_size=0.2)
    train_X, valid_X, train_label, valid_label = train_test_split(train_X, train_label, test_size=0.2)

    # Create PyTorch DataLoader objects
    train_dataset = CustomDataset(train_X, train_label)
    train_loader = DataLoader(train_dataset, batch_size=batch_size, shuffle=True)
    valid_dataset = CustomDataset(valid_X, valid_label)
    valid_loader = DataLoader(valid_dataset, batch_size=batch_size, shuffle=False)
    test_dataset = CustomDataset(test_X, test_label)
    test_loader = DataLoader(test_dataset, batch_size=batch_size, shuffle=True)

    model = TCPDumpModel(num_classes=num_classes,
                         rows=train_X.shape[1],
                         cols=1,
                         nb_filters=32,
                         psize=6,
                         ksize=16)
    
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=0.01)

    return train_test_model(model, train_loader, test_loader, optimizer, criterion, epoch_num)