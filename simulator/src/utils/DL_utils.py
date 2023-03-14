import numpy as np
import pandas as pd


from sklearn.model_selection import train_test_split
import tensorflow as tf
from tensorflow import keras as keras
from tensorflow.keras import layers
from tensorflow.keras import backend as K
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense, Conv1D, Flatten, MaxPooling1D, Dropout
from tensorflow.keras.utils import to_categorical

tf.get_logger().setLevel('INFO')
configtf = tf.compat.v1.ConfigProto()
configtf.gpu_options.allow_growth=True
configtf.gpu_options.per_process_gpu_memory_fraction = 0.6

import configlib

def normalization_l2(df):
    x = df.loc[:, df.columns != 'label']
    y = df['label']
    norm = (np.sqrt(np.square(df).sum(axis=1)))
    normalized_x = x/norm
    normalized_df = pd.concat([normalized_x, y], axis = 1)
    return normalized_df

def build_model_tcpdump(num_classes, rows, cols, nb_filters, psize, ksize):
    model = Sequential()
    model.add(Conv1D(nb_filters, kernel_size=ksize, activation='relu', input_shape=(rows, cols)))
    model.add(Conv1D(nb_filters, kernel_size=ksize, activation='relu'))
    model.add(Conv1D(nb_filters, kernel_size=ksize, activation='relu'))
    model.add(Dropout(rate=0.3))
    model.add(MaxPooling1D(pool_size=psize))
    model.add(Dropout(rate=0.2))
    model.add(Flatten())
    model.add(Dense(64, activation='relu'))
    model.add(Dropout(rate=0.2))
    model.add(Dense(num_classes, activation='softmax'))
    model.compile(optimizer='adam', loss='categorical_crossentropy',
        metrics=['accuracy'])
    return model


def train_test_and_report_acc(config: configlib.Config, df):
    epoch_num = config.attack_epoch_num
    batch_size = config.attack_batch_size
    num_classes = config.num_of_unique_streams
    # Dataset Normalization 
    normalized_df = normalization_l2(df)
    normalized_df = df
    # Extracting labels
    x = df.loc[:, df.columns != 'label']
    y = df['label']

    # One hot encoding and splitting dataset to test and train
    one_hot_encoded_y = to_categorical(df.label)
    train_X,test_X,train_label,test_label =  train_test_split(x, one_hot_encoded_y, test_size=0.2)
    train_X,valid_X,train_label,valid_label =  train_test_split(train_X, train_label, test_size=0.2)
    row = train_X.shape[1]

    # building the model  
    model = build_model_tcpdump(num_classes=num_classes, rows = row, cols=1, nb_filters=32, psize=6, ksize=16)

    # Training the model
    model.fit(train_X, train_label, batch_size=batch_size,epochs=epoch_num,verbose=0,validation_data=(valid_X, valid_label))

    # Testing the model
    _, acc = model.evaluate(test_X, test_label, verbose=0)

    return acc