"""
Example Python script that:
  1) Reads electrode data from a serial port (line by line).
  2) Applies three filters in sequence:
     a) 50 Hz notch filter for power-line interference.
     b) Teager–Kaiser Energy (TKE) operator.
     c) Moving average.
  3) Plots the filtered results in an animation, restricting the Y-axis from -550 to +550.

Dependencies:
  pip install pyserial numpy matplotlib scipy
Adjust the COM port, baud rate, and other parameters as needed.
"""

import serial
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from scipy.signal import iirnotch, filtfilt

########################
# Filtering Functions  #
########################

def notch_filter_50Hz(signal, fs=1000.0, quality=60.0):
    """Apply a 50 Hz notch filter to each column in 'signal'.
    :param signal: 2D numpy array of shape (samples, channels)
    :param fs: Sampling frequency (Hz)
    :param quality: Q-factor for the notch filter
    :return: Filtered 2D numpy array
    """
    # Design a notch filter at 50 Hz
    w0 = 50.0 / (fs / 2.0)  # Normalized frequency
    b, a = iirnotch(w0, quality)

    # Filter each channel
    filtered = np.zeros_like(signal)
    for ch in range(signal.shape[1]):
        filtered[:, ch] = filtfilt(b, a, signal[:, ch])

    return filtered


def teager_kaiser_energy(signal):
    """Apply the Teager-Kaiser Energy operator to each column.
    TKE formula for x[n]: y[n] = x[n]^2 - x[n+1]*x[n-1].
    We'll set boundary points (0, -1) to zero for simplicity.

    :param signal: 2D numpy array of shape (samples, channels)
    :return: 2D numpy array of TKE outputs
    """
    tke_out = np.zeros_like(signal)
    for ch in range(signal.shape[1]):
        x = signal[:, ch]
        for n in range(1, len(x) - 1):
            tke_out[n, ch] = x[n] * x[n] - x[n + 1] * x[n - 1]
        # The first and last remain 0.
    return tke_out


def moving_average(signal, window_size=5):
    """Apply a simple moving average to each column.

    :param signal: 2D numpy array of shape (samples, channels)
    :param window_size: Number of samples for the moving average
    :return: 2D numpy array of smoothed outputs
    """
    smoothed = np.zeros_like(signal)
    for ch in range(signal.shape[1]):
        x = signal[:, ch]
        # Use 'same' mode so the output has the same length.
        smoothed[:, ch] = np.convolve(x, np.ones(window_size) / window_size, mode='same')
    return smoothed

########################
# Serial Data Function #
########################

def read_serial_data(port="COM7", baudrate=250000, num_samples=1000):
    """Reads electrode data from a serial port.
    Expects each line to have values separated by semicolons, e.g. "v1;v2;...;vN".
    This function collects 'num_samples' lines from the port.

    :param port: Serial port name, e.g. 'COM3' or '/dev/ttyUSB0'.
    :param baudrate: Baud rate for the serial port
    :param num_samples: Number of lines to read
    :return: 2D numpy array of shape (num_samples, num_channels)
    """
    ser = serial.Serial(port, baudrate, timeout=1)
    all_data = []

    print(f"Reading {num_samples} samples from serial port {port}...")

    for _ in range(num_samples):
        line = ser.readline().decode('utf-8', errors='replace').strip()
        if not line:
            continue
        # Parse semicolon-separated floats
        parts = line.split(',')
        print(parts)
        try:
            row = [float(p) for p in parts]
            all_data.append(row)
        except:
            # If parse fails, skip
            pass

    ser.close()

    data_array = np.array(all_data)
    return data_array


###############################
# Main Script (with Animation)#
###############################

def main():
    # 1) Read raw data from serial
    # Adjust these parameters as needed
    serial_port = "COM3"
    baud = 250000
    num_samples = 50000

    raw_data = read_serial_data(port=serial_port, baudrate=baud, num_samples=num_samples)
    if raw_data.size == 0:
        print("No data was read from the serial port.")
        return

    print(f"Raw data shape: {raw_data.shape}")

    # 2) Apply filters in sequence
    #    a) 50 Hz notch filter
    #    b) TKE
    #    c) Moving average

    # Estimate or define your sampling frequency. Adjust if known.
    fs = 1000.0
    
    raw_data = raw_data[:,:-2]

    notched_data = notch_filter_50Hz(raw_data, fs=fs, quality=30.0)
    tke_data = teager_kaiser_energy(raw_data)
    filtered_data = moving_average(tke_data, window_size=5)
    #filtered_data = notched_data
    # 3) Animate the data
    fig, ax = plt.subplots()
    print(filtered_data.shape)
    num_samples, num_ch = filtered_data.shape

    # We'll plot each channel on the same figure
    lines = []
    colors = plt.cm.viridis(np.linspace(0, 1, num_ch))
    for ch in range(num_ch):
        (line,) = ax.plot([], [], color=colors[ch], label=f"Channel {ch+1}")
        lines.append(line)

    # Fix X range from 0 to num_samples, Y range from -550 to 550
    ax.set_xlim(0, num_samples)
    ax.set_ylim(-550, 550)
    ax.set_xlabel("Sample")
    ax.set_ylabel("Filtered Value")
    ax.legend()

    def init():
        for line in lines:
            line.set_data([], [])
        return lines

    def update(frame):
        # Frame is the index up to which we draw
        x_vals = np.arange(frame)
        for ch, line in enumerate(lines):
            y_vals = filtered_data[:frame, ch]
            line.set_data(x_vals, y_vals)
        return lines

    ani = animation.FuncAnimation(
        fig,
        update,
        frames=num_samples,
        init_func=init,
        blit=True,
        interval=30  # Adjust animation speed if needed
    )

    plt.show()


if __name__ == "__main__":
    main()
