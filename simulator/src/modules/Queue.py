class Queue():
  def __init__(self, max_size, min_size):
      self.size = 0
      self.status = "empty"
      self.max_size = max_size
      self.min_size = min_size

  def get_size(self):
    return self.size

  def flush(self):
    self.size = 0
    self.status = "empty"

  def enqueue(self, data_size):
    if(self.size + data_size > self.max_size):
      print("Cannot enqueue, queue doesn't have enough capacity")
      return -1
    self.size = self.size + data_size
    if(self.size > 0):
      self.status = "non-empty"
    return self.size
  
  def dequeue(self, data_size):
    if(self.size - data_size < self.min_size):
      self.size = 0
    else:
      self.size = self.size - data_size
    if(self.size == 0):
      self.status = "empty"
    return self.size
  
  def get_status(self):
    return self.status



