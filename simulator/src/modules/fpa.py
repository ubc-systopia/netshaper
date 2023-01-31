import math
import numpy as np
import pandas as pd

def FPA_mechanism(app_data, epsilon, sensitivity, k):
  nonPrivate_arr = app_data.to_numpy()
  private_arr = []
  for idx, line in enumerate(nonPrivate_arr):
    retQ = list(fpa(line, epsilon, sensitivity, k)) 
    private_arr.append(retQ)
  private_df = pd.DataFrame(private_arr)
  return private_df

def fpa(Q, epsilon, l2sen, k=10, printflag=False):
  clip_top = 1e9
  clip_bottom = 0
  lam = math.sqrt(k)*l2sen/epsilon

  #DFT
  fftold = np.fft.rfft(Q)
  n = len(fftold)
  fftnow = fftold[:k]

  #laplace
  noise_real = np.random.laplace(0,lam,np.shape(fftnow))
  noise_imag = np.random.laplace(0,lam,np.shape(fftnow))

  fftnow.real += noise_real
  fftnow.imag += noise_imag
  # print ("fftnow after adding noise:",fftnow, np.shape(fftnow))

  #pad
  padlen = n - k
  fftpad = np.pad(fftnow, pad_width=padlen, mode='constant', constant_values=0)[padlen:]
  # print ("after padding:",fftpad, np.shape(fftpad))

  #IDFT
  retQ = np.fft.irfft(fftpad)
  retQ = np.clip(retQ,clip_bottom,clip_top)
  if printflag:
    print ("original sum:",sum(Q),"IDFT sum:",sum(retQ))
  return retQ