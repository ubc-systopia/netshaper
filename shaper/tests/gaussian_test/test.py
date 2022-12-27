import numpy as np
import matplotlib.pyplot as plt



sample_size = 10000
mu = 1.3
sigma = 7;
x = np.random.normal(mu, sigma, sample_size)
# read the data from the file sample.txt and plot as a histogram
y = np.loadtxt('/home/ubuntu/workspace/minesVPN/shaper/tests/gaussian_test/samples.txt')
plt.hist(y, bins=100, density=True, alpha=0.5, label='sample.txt')
# plot the distribution
plt.hist(x, bins=100, density=True, alpha=0.5, label='normal')
plt.legend(loc='upper right')
plt.show()
plt.savefig('/home/ubuntu/workspace/minesVPN/shaper/tests/gaussian_test/gaussian.pdf')