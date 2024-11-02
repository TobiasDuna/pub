import serial
import time

# Konfigurer seriell port
ser = serial.Serial('COM3', 115200)
tid_før = time.perf_counter()
# Åpne fil for å lagre data
with open("temp_log.txt", "w") as file:
    file.write("Tid, Temp \n")

    while time.perf_counter() - tid_før < 15000:
        if ser.in_waiting > 0:
            data = ser.readline().decode().strip()
            print(data)
            file.write(str(data) + '\n' )
            file.flush()
ser.close()