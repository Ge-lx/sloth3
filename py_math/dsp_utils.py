import numpy as np
from scipy import signal, fft, linalg
from matplotlib import pyplot as plt

def white_noise (n: int, offset: float = 0, scale: float = 1, dtype: np.dtype = float):
    return ((np.random.random(n) - 0.5) * scale + offset).astype(dtype)

def fourier (x, f_s):
    f = fft.fft(x)
    f_range = fft.fftfreq(len(x), 1/f_s)
    return (f, f_range)

def sinad (before, after):
    diff = after - before
    return 10 * np.log10(np.sum((after - np.mean(after))**2) / np.sum(diff**2))

def enob (before, after):
    return (sinad(before, after) - 1.76) / 6.02

def find_delay (after, before, dly_range, plot=False):
    len_diff = len(after) - len(before)
    if (len_diff > 0):
        after = after[abs(len_diff):]
    elif (len_diff < 0):
        before = before[abs(len_diff):]
    
    match_for_dly = np.zeros(dly_range[1] - dly_range[0])
    for i, d in enumerate(range(*dly_range)):
        if (d < 0):
            diff = before[(-d):] - after[:-(-d)]
        elif (d > 0):
            diff = after[d:] - before[:-d]
        else:
            diff = after - before
        # diff =  if d != 0 else after - before
        match_for_dly[i] = np.sum(np.abs(diff))    
    best_match = range(*dly_range)[np.argmin(np.abs(match_for_dly))]
    
    if (plot):
        plt.figure(figsize=(6,3))
        plt.plot(range(*dly_range), match_for_dly)
        plt.xlabel('Delay [samples]')
        plt.title(f'Best match at delay = {best_match} samples')
        plt.show()
    
    if (best_match < 0):
        return after[:-(-best_match)], before[(-best_match):]
    elif (best_match > 0):
        return after[best_match:], before[:-best_match]
    else:
        return after, before
    
def convmtx(h,n):
    # creates the convolution (Toeplitz) matrix, which transforms convolution to matrix multiplication
    # h is input array, 
    # n is length of array to convolve 
    return linalg.toeplitz(np.hstack([h, np.zeros(n-1)]), np.hstack([h[0], np.zeros(n-1)]))

# Compute equalizer given known tx and rx waveforms
def optimize_filter (tx, rx, num_taps=100, omit=500, shift=0, depth=100):
    # omit : initial samples to exclude
    # shift: number of samples to shift dominant equalizer tap to the right

    depth_samp = num_taps * depth
    delay_samp = num_taps//2

    A = convmtx(rx[omit+shift: omit+shift+depth_samp], num_taps)
    R = np.dot(np.conj(A).T, A)
    X = np.concatenate([np.zeros(delay_samp), tx[omit: omit+depth_samp], np.zeros(int(np.ceil(num_taps/2)-1))])
    ro = np.dot(np.conj(A).T, X)  
    
    return np.dot(linalg.inv(R),ro)