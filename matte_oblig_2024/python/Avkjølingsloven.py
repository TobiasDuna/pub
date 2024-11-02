import numpy as np
import matplotlib.pyplot as plt
from scipy.ndimage import gaussian_filter1d

def analytisk_løsning(t, T_0, alpha, T_k):                         # Analytisk løsning av T'(t) = alpha(T_k - T(t))
    return T_k + (T_0 - T_k) * np.exp(-alpha * t)                  # C = (T_0 - T_k)

def plot_newtons_avkjøling(T_0, T_1, T_k, tid, dt): 
    dT_0 = T_0 - T_k
    dT_1 = T_1 - T_k
    global alpha
    alpha = -np.log(dT_1 / dT_0) / dt                              # Beregn alpha ut ifra initialverdier
    t = np.linspace(tid[0], tid[1], int(tid[1] / 5))
    T = analytisk_løsning(t, T_0, alpha, T_k) 

    plt.plot(t, T, label="Analytisk Vanntemperatur", color="green")
    plt.axhline(T_k, color='gray', linestyle='--',label=f"Gj.sn. lufttemperatur" )
    plt.xlabel('Tid (min)')
    plt.ylabel('Temperatur (C)')
    plt.title("Newtons avkjølingslov")
    plt.legend()
    plt.grid()

måling_liste = []

with open('temp_log.txt', 'r') as log:
    for linje in log:
        data = linje.strip().split(',')
        t, øl, luft = map(float, data)
        måling_liste.append([t, luft, øl])                         # Les målinger fra fil og konverter til floats

måling_liste = np.array(måling_liste)

# Hent initialverdier fra måledata
T_0 = måling_liste[0, 2]                                           # Første måleverdi for øltemperatur
T_1 = måling_liste[len(måling_liste) // 2, 2]                      # Midterste måleverdi for øltemperatur
T_k = np.mean(måling_liste[:, 1])                                  # Gjennomsnittlig omgivelsestemperatur
dt = måling_liste[len(måling_liste) // 2, 0] - måling_liste[0, 0]  # Tid mellom første og midterste måling
tid = (0, måling_liste[-1, 0])                                     # Start- og sluttid basert på dataene

def plot_målinger(målinger):
    plt.plot(målinger[:, 0]-målinger[0,0], målinger[:, 2], color='red', label='Målt Vanntemperatur') #Trekker fra første timestamp
    plt.plot(målinger[:, 0]-målinger[0,0], målinger[:, 1], color='blue', label='Målt Lufttemperatur')   #for å starte på 0

def plot_smooth_målinger(målinger):
    smooth_øl = gaussian_filter1d(målinger[:, 2], sigma=10)        # Glatte ut verdiene med SciPy   
    smooth_luft = gaussian_filter1d(målinger[:, 1], sigma=10)         
    plt.plot(målinger[:, 0]-målinger[0,0], smooth_øl, color='red', label='Glattet Vanntemperatur')  
    plt.plot(målinger[:, 0]-målinger[0,0], smooth_luft, color='blue', label='Glattet Lufttemperatur')

# Plot Newtons avkjølingslov og målingene
plot_newtons_avkjøling(T_0, T_1, T_k, tid, dt)
plot_målinger(måling_liste)
#plot_smooth_målinger(måling_liste)
print(alpha)
plt.legend()
#plt.savefig('Rå_data.png')
plt.show()