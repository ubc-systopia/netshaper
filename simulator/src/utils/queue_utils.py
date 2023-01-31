def check_all_queues_empty(queue_list):
  results = [queue.get_status() == "empty" for queue in queue_list]
  return all(results)

def get_queue_sizes(queue_list):
  return [queue.get_size() for queue in queue_list]

def queues_push(data, queues_list):
  assert(len(data) == len(queues_list), "The length of data and queues_list should be the same.")
  for i in range(len(queues_list)):
    if(queues_list[i].enqueue(data[i]) < 0 ):
      print("Something is wrong with queue: %d, cannot enqueue!" % (i))
      return -1 
  return 0
