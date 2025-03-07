import numpy as np
import scipy.signal as signal
import librosa
import librosa.display
import soundfile as sf
import matplotlib.pyplot as plt

def notch_filter(data, sr, notch_freq, quality_factor=30):
    """Lager et Notch-filter og bruker det på signalet."""
    w0 = notch_freq / (sr / 2)  # Normalisert frekvens
    b, a = signal.iirnotch(w0, quality_factor)
    filtered_signal = signal.filtfilt(b, a, data)
    return filtered_signal

# Laste inn lydfil
filename = "Lydsignalfør.wav"
y, sr = librosa.load(filename, sr=None)  # Leser lydfilen med original samplingsrate

# Notch-filter for 760 Hz
notch_freq = 760  # Frekvensen som skal fjernes
filtered_y = notch_filter(y, sr, notch_freq)

# Lagre den filtrerte lyden
sf.write("filtrert.wav", filtered_y, sr)

# Plotting av spektrum før og etter filtrering
plt.figure(figsize=(12, 6))

plt.subplot(2, 1, 1)
plt.title("Spektrum før filtrering")
plt.magnitude_spectrum(y, Fs=sr, scale='dB', color='blue')

plt.subplot(2, 1, 2)
plt.title("Spektrum etter filtrering")
plt.magnitude_spectrum(filtered_y, Fs=sr, scale='dB', color='red')

plt.tight_layout()
plt.show()

print("Filtrert lyd lagret som 'filtered_output.wav'")