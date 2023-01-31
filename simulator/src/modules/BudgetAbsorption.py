import numpy as np

class BudgetAbsorption():
  def __init__(self, w, epsilon, decision_publication_ratio, S):
    self.w = w
    self.epsilon = epsilon
    self.current_indx = 0

    self.publication_epsilon = (epsilon * (1-decision_publication_ratio)) / w 
    self.decision_epsilon = (epsilon * decision_publication_ratio) / w
    self.sensitivity = S
    self.assigned_epsilons = []

  def laplace_mechanism(self, query, sensitivity, epsilon):
    return np.random.laplace(query, sensitivity/epsilon)
  
  def calculate_DP_dist(self, recent_DP_output, current_nonDP_output):
    actual_dist = abs(recent_DP_output - current_nonDP_output)
    DP_dist = self.laplace_mechanism(actual_dist, self.sensitivity, self.decision_epsilon)
    return DP_dist

  def initialization_wDP_mechanism(self, query):
    # Here we have to publish the first value as there is no previous publication to 
    ## used for approximation.
    DP_output = self.laplace_mechanism(query, self.sensitivity, self.publication_epsilon)
    self.assigned_epsilons.append(self.publication_epsilon)
    self.recent_DP_output = DP_output
    self.recent_output_indx = self.current_indx
    self.drained_indx = self.current_indx 
    return DP_output
 
  def wDP_mechanism(self, query):
    # This means there is no budget for a new publication because the previous one has absorbed it.
    budget_is_nullified = (self.current_indx <= self.drained_indx)
    if budget_is_nullified:
      self.assigned_epsilons.append(0)
      return self.recent_DP_output
    DP_dist = self.calculate_DP_dist(self.recent_DP_output, query) 

    unused_budget_count = min(self.current_indx - self.drained_indx, self.w)
    epsilon_to_absorb =  unused_budget_count * self.publication_epsilon 
    # This part checks weather the available budget is enough to provide a good publication or not.
    ## If yes, it is going to publish a new value and update all related indices.
    if (DP_dist < 1/epsilon_to_absorb) :
      self.assigned_epsilons.append(0)
      return self.recent_DP_output
    else:
      self.assigned_epsilons.append(epsilon_to_absorb)
      DP_output = self.laplace_mechanism(query, self.sensitivity, epsilon_to_absorb)
      self.recent_DP_output = DP_output
      self.recent_output_indx = self.current_indx
      self.drained_indx = self.current_indx + unused_budget_count - 1
      return DP_output

  def make_query_wDP(self, query):
    if(self.current_indx == 0):
      DP_output = self.initialization_wDP_mechanism(query)
    else:
      DP_output = self.wDP_mechanism(query)

    self.current_indx += 1
    return DP_output

  def get_assigned_epsilons(self):
    return self.assigned_epsilons