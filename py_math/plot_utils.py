import dsp_utils as dsp
import numpy as np
from scipy import signal
from matplotlib import ticker
from matplotlib import pyplot as plt

def signal_plot (x: np.array, f_s: float, cb: callable, window: np.array = None , ):

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 10))

    def plot_once (i, x):
        x_win = x.copy()
        if window is not None:
            x = x * window    

        t_range = np.linspace(0, len(x) / f_s, len(x), endpoint=False)
        f, f_range = dsp.fourier(x_win, f_s)

        ax1.step(t_range, np.real(x), label=f'real({i})')
        ax1.step(t_range, np.imag(x), label=f'imag({i})')
    

        ax2.step(f_range, np.real(f), label=f'real({i})')
        ax2.step(f_range, np.imag(f), label=f'imag({i})')
        # ax2.step(f_range, np.abs(f))
        # ax2.fill_between(f_range, np.zeros(len(f)), np.real(f))
        # ax2.fill_between(f_range, np.zeros(len(f)), np.imag(f))
        ax1.legend()
        ax2.legend()
        cb(ax1, ax2, fig, f=f, x=x)



    if isinstance(x, tuple):
        [plot_once(*x_) for x_ in enumerate(x)]
    else:
        plot_once(0, x)

    plt.show()

def plot_filter (filt, f_s, cb: callable = lambda *args: None):
    w, h_bp = signal.freqz(filt, fs = f_s, worN=2048)

    fig, (ax1) = plt.subplots(1, 1, figsize=(5, 3))

    ax2 = ax1.twinx()
    ax1.grid(True, 'both', 'both')
    ax1.plot(w, 20 * np.log10(np.abs(h_bp)), lw=1, color='C1', label='filter gain')
    w, g_delay = signal.group_delay((filt, 1), w=w, fs=f_s)
    ax2.plot(w, g_delay, lw=1, ls='dashed', label='group delay')
    cb(ax1, ax2, fig)
    plt.legend()
    plt.show()

