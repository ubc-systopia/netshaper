import argparse
import pprint
import json
from typing import Dict, Any
import numpy as np
import pandas as pd
import sys






# Defining a global parser
parser = argparse.ArgumentParser(description="parsing simulator configs", argument_default=argparse.SUPPRESS)

# Adding arguments to the parser
p = parser.add_argument_group('configs')

# p.add_argument('--total_loss', type=float, metavar="X", help='Total privacy loss')
# p.add_argument('--num_of_queries', type=int, metavar="Y", help='Number of queries')
# p.add_argument('--delta', type=float, metavar="Z", help='Delta')


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

if __name__ == "__main__":
    # parser = argparse.ArgumentParser(description="parsing simulator configs", argument_default=argparse.SUPPRESS)

    # Adding arguments to the parser
    p = parser.add_argument_group('configs')
    p.add_argument('--total_loss', type=float, default=None, metavar="X", help='Total privacy loss')
    p.add_argument('--num_of_queries', type=int, metavar="Y", help='Number of queries')
    p.add_argument('--delta', type=float, metavar="Z", help='Delta')
    p.add_argument('--noise_multiplier', type=float, default=None, metavar="Z", help='Noise Multiplier')
    args = parser.parse_args()

    ## Checking the number of arguments is correct
    if len(vars(args)) != 4:
        print("Usage (First Format): python3 privacy_calculator.py --total_loss X --num_of_queries Y --delta Z")
        print("Usage (Second Format): python3 privacy_calculator.py --noise_multipier X --num_of_queries Y --delta Z")
        exit() 
    
    alphas = [1 + x / 10.0 for x in range(1, 100)] + list(range(12, 64))
    noise_multipliers = [3, 6, 12, 25, 50]
    if (args.total_loss is not None and args.noise_multiplier is not None):
        print("You can either specify the total privacy loss or the noise multiplier. Not both.")
        exit(1)
    
    if (args.noise_multiplier is not None):    
        total_loss = calculate_privacy_loss(args.num_of_queries, alphas, args.noise_multiplier, args.delta)
        print("total_loss:",total_loss[0])
    elif (args.total_loss is not None):
        noise_multiplier = get_noise_multiplier(args.total_loss, args.num_of_queries, alphas, args.delta)
        print("noise multiplier:", noise_multiplier)