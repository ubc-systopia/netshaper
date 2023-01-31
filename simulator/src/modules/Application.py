import pandas as pd

class Application():
  def __init__(self, application_window_size, data, data_window_size):
    if not isinstance(data, pd.DataFrame):
      raise TypeError("The data should be a pandas dataframe.")
    if not (application_window_size % data_window_size == 0):
      raise ValueError(f'The application window size should be a multiple of the data window size.')
    self.app_df = data
    self.app_T = application_window_size
    self.app_data = data.loc[:, data.columns != 'label']
    self.app_labels = data['label']
    self.data_pointer = 0
    self.app_status = "ongoing" # two status: 1-ongoing, 2-terminated
  
  def data_loader(self):
    return self.app_df
  
  def generate_data(self, step):
    if (self.get_status == "terminated"):
      return -1
    start_ind = self.data_pointer
    if(self.data_pointer + step > len(self.app_data.columns) - 1):
      end_ind = len(self.app_data.columns) - 1
      self.set_status("terminated")
    else:
      end_ind = self.data_pointer + step
      self.set_status("ongoing")
    data_slot = self.app_data.iloc[:, start_ind: end_ind].sum(axis=1)
    self.data_pointer = end_ind
    return data_slot.to_numpy()

  def set_status(self, status):
    self.app_status = status

  def get_status(self):
    return self.app_status
  
  def get_num_of_streams(self):
    return len(self.app_data)
  
  def get_all_data(self):
    return self.app_data

  def get_all_labels(self):
    return self.app_labels

  def get_stream_len(self):
    return len(self.app_data.columns)
