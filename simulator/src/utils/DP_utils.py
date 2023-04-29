import numpy as np
import pandas as pd
from src.utils.queue_utils import *


def calculate_sensitivity_using_dataset(app_data):
  max_colBased = app_data.max()
  min_colBased = app_data.min()
  diff = max_colBased - min_colBased
  print(len(diff))
  sensitivity = max(diff.to_numpy())
  return sensitivity

def laplace_mechanism(value, l1_sensitivity, epsilon):
  mu = 0
  gamma = l1_sensitivity/epsilon
  DP_value = value + np.random.laplace(mu, gamma, 1)[0]
  return DP_value

def gaussian_mechanism(value, l2_sensitivity, epsilon, delta=1e-5):
  mu = 0
  sigma = (l2_sensitivity/epsilon) * np.sqrt(2*np.log(1.25/delta))
  DP_value = value + np.random.normal(mu, sigma, 1)[0]
  return DP_value

def gaussian_mechanism_rdp(value, l2_sensitivity, noise_multiplier ):
  mu = 0
  sigma = l2_sensitivity * noise_multiplier
  DP_value = value + np.random.normal(mu, sigma, 1)[0]
  return DP_value

def queues_pull_DP(queues_list, epsilon, sensitivity, DP_mechanism, noise_multiplier, min_DP_size, max_DP_size):
  
  DP_data_sizes = []
  dummy_sizes = []
  for i in range(len(queues_list)):
    true_size = queues_list[i].get_size()

    # Making DP decision about dequeue size
    #DP_size = true_size + random.choice([-10,0])
    if(DP_mechanism == "Gaussian"):
      DP_size = gaussian_mechanism(true_size, sensitivity, epsilon, 1e-5)
    elif(DP_mechanism == "Laplace"):
      DP_size = laplace_mechanism(true_size, sensitivity, epsilon)
    elif(DP_mechanism == "Gaussian_rdp"):
      DP_size = gaussian_mechanism_rdp(true_size, sensitivity, noise_multiplier)

    DP_size = min_DP_size if DP_size < min_DP_size else DP_size
    DP_size = max_DP_size if DP_size > max_DP_size else DP_size
    
    DP_data_sizes.append(DP_size)

    # Updating queue size
    if(DP_size > true_size):
      queues_list[i].dequeue(true_size)
      dummy_size = DP_size - true_size
    else:
      queues_list[i].dequeue(DP_size)
      dummy_size = 0
    dummy_sizes.append(dummy_size)
  return DP_data_sizes, dummy_sizes

def queues_pull_wDP(queues_list, wPrivacy_modules, sensitivity, DP_mechanism):
  DP_data_sizes = []
  for i in range(len(queues_list)):
    true_size = queues_list[i].get_size()

    # Making DP decision about queue size
    if(DP_mechanism == "Gaussian"):
      pass
    elif(DP_mechanism == "Laplace"):
      DP_size = wPrivacy_modules[i].make_query_wDP(true_size)  

    DP_size = 0 if DP_size < 0 else DP_size
    DP_data_sizes.append(DP_size)

    # Updating queue size
    if(DP_size > true_size):
      queues_list[i].dequeue(true_size)
    else:
      queues_list[i].dequeue(DP_size)
  return DP_data_sizes
  
def check_BudgetAbsorption_correctness(epsilons_df, w):
  upper_bound_indx = len(epsilons_df.columns) - w - 1
  w_epsilons = []
  for i in range(upper_bound_indx):
    w_epsilon = epsilons_df.iloc[:,i:i+w].sum(axis=1)
    w_epsilons.append(list(w_epsilon))
    
def get_all_assigned_epsilons(wPrivacy_modules):
  epsilons_df = []
  for wPrivacy_module in wPrivacy_modules:
    epsilons_df.append(wPrivacy_module.get_assigned_epsilons())
  epsilons_df = pd.DataFrame(epsilons_df)
  return epsilons_df


# def FPA_mechanism(app_data, epsilon, sensitivity, k):
#   nonPrivate_arr = app_data.to_numpy()
#   private_arr = []
#   for idx, line in enumerate(nonPrivate_arr):
#     retQ = list(fpa(line, epsilon, sensitivity, k)) 
#     private_arr.append(retQ)
#   private_df = pd.DataFrame(private_arr)
#   return private_df

def get_noise_multiplier(total_loss, num_of_queries, alphas, delta):
  threshold = 0.01
  tmp_ans = 0
  min_ans = 0
  max_ans = 1000
  tmp_total_loss = calculate_privacy_loss(num_of_queries, alphas, tmp_ans, delta)[0]
  while (abs(tmp_total_loss - total_loss) > threshold):
    tmp_ans = (min_ans + max_ans) / 2
    tmp_total_loss = calculate_privacy_loss(num_of_queries, alphas, tmp_ans, delta)[0]
    if(tmp_total_loss > total_loss):
      min_ans = tmp_ans
    else:
      max_ans = tmp_ans
  return tmp_ans

# Renyi Differential Privacy
import math
import numpy as np
from scipy import special


# Code borrowed from Opacus


'''
Given the number of steps, calculate the privacy loss with Rényi-DP
'''


def calculate_privacy_loss(steps, alphas, sigma, delta):
    q = 1
    noise_multiplier = sigma
    rdp = compute_rdp(q, noise_multiplier, steps, alphas)
    eps, best_alpha = get_privacy_spent(alphas, rdp, delta)
    return eps, best_alpha


'''
Computes epsilon given a list of Renyi differential privacy values
at multiple RDP orders and target delta

orders: Array or scalar of alphas

rdp: List or scalar of RDP guarantees

delta: Target delta
'''


def get_privacy_spent(orders, rdp, delta):
    orders_vec = np.atleast_1d(orders)
    rdp_vec = np.atleast_1d(rdp)

    if len(orders_vec) != len(rdp_vec):
        raise ValueError(
            f"Input lists must have the same length.\n"
            f"\torders_vec = {orders_vec}\n"
            f"\trdp_vec = {rdp_vec}\n"
        )

    eps = rdp_vec - math.log(delta) / (orders_vec - 1)

    # special case when there is no privacy
    if np.isnan(eps).all():
        return np.inf, np.nan

    idx_opt = np.nanargmin(eps)  # Ignore NaNs
    return eps[idx_opt], orders_vec[idx_opt]


'''
Computes Rényi Differential Privacy guaranteed of gaussian mechanism
iterated "steps" times.

q: Sampling rate of gaussian mechanism

noise_multiplier: Ratio of standard deviation of additive Gaussian

noise to the L2-sensitivity of the function to which it is added.

steps: The number of iterations of the mechanism

orders: Array or scalar of alphas
'''


def compute_rdp(q, noise_multiplier, steps, orders):
    if isinstance(orders, float):
        rdp = _compute_rdp(q, noise_multiplier, orders)
    else:
        rdp = np.array([_compute_rdp(q, noise_multiplier, order) for order in orders])

    return rdp * steps


'''
Computes RDP of the gaussian mechanism at alpha

q: Sampling rate of gaussian mechanism

sigma: Standard deviation of the additive Gaussian noise

alpha: The order at which RDP is computed
'''


def _compute_rdp(q, sigma, alpha):
    if q == 0:
        return 0

    # no privacy
    if sigma == 0:
        return np.inf

    if q == 1.0:
        return alpha / (2 * sigma ** 2)

    if np.isinf(alpha):
        return np.inf

    return _compute_log_a(q, sigma, alpha) / (alpha - 1)


def _compute_log_a(q, sigma, alpha):
    if float(alpha).is_integer():
        return _compute_log_a_for_int_alpha(q, sigma, int(alpha))
    else:
        return _compute_log_a_for_frac_alpha(q, sigma, alpha)


def _compute_log_a_for_frac_alpha(q, sigma, alpha):
    # The two parts of A_alpha, integrals over (-inf,z0] and [z0, +inf), are
    # initialized to 0 in the log space:
    log_a0, log_a1 = -np.inf, -np.inf
    i = 0
    z0 = sigma ** 2 * math.log(1 / q - 1) + 0.5

    while True:  # do ... until loop
        coef = special.binom(alpha, i)
        log_coef = math.log(abs(coef))
        j = alpha - i

        log_t0 = log_coef + i * math.log(q) + j * math.log(1 - q)
        log_t1 = log_coef + j * math.log(q) + i * math.log(1 - q)

        log_e0 = math.log(0.5) + _log_erfc((i - z0) / (math.sqrt(2) * sigma))
        log_e1 = math.log(0.5) + _log_erfc((z0 - j) / (math.sqrt(2) * sigma))

        log_s0 = log_t0 + (i * i - i) / (2 * (sigma ** 2)) + log_e0
        log_s1 = log_t1 + (j * j - j) / (2 * (sigma ** 2)) + log_e1

        if coef > 0:
            log_a0 = _log_add(log_a0, log_s0)
            log_a1 = _log_add(log_a1, log_s1)
        else:
            log_a0 = _log_sub(log_a0, log_s0)
            log_a1 = _log_sub(log_a1, log_s1)

        i += 1
        if max(log_s0, log_s1) < -30:
            break

    return _log_add(log_a0, log_a1)


def _compute_log_a_for_int_alpha(q, sigma, alpha):
    # Initialize with 0 in the log space.
    log_a = -np.inf

    for i in range(alpha + 1):
        log_coef_i = (
            math.log(special.binom(alpha, i))
            + i * math.log(q)
            + (alpha - i) * math.log(1 - q)
        )

        s = log_coef_i + (i * i - i) / (2 * (sigma ** 2))
        log_a = _log_add(log_a, s)

    return float(log_a)


def _log_add(logx, logy):
    a, b = min(logx, logy), max(logx, logy)
    if a == -np.inf:  # adding 0
        return b
    # Use exp(a) + exp(b) = (exp(a - b) + 1) * exp(b)
    return math.log1p(math.exp(a - b)) + b  # log1p(x) = log(x + 1)


def _log_sub(logx, logy):
    if logx < logy:
        raise ValueError("The result of subtraction must be non-negative.")
    if logy == -np.inf:  # subtracting 0
        return logx
    if logx == logy:
        return -np.inf  # 0 is represented as -np.inf in the log space.

    try:
        # Use exp(x) - exp(y) = (exp(x - y) - 1) * exp(y).
        return math.log(math.expm1(logx - logy)) + logy  # expm1(x) = exp(x) - 1
    except OverflowError:
        return logx


def _log_erfc(x):
    return math.log(2) + special.log_ndtr(-x * 2 ** 0.5)
