import serial
import threading
import sys
import time
import glob
from datetime import datetime

# Descubrimiento automático de todos los puertos /dev/ttyACM* activos
ports = sorted(glob.glob("/dev/ttyACM*"))
baudrate = 115200

# Paleta de colores rotativa para diferenciar los nodos en consola
colors = [
    "\033[93m", # Amarillo (Nodo 1)
    "\033[96m", # Cian (Nodo 2)
    "\033[92m", # Verde (Nodo 3)
    "\033[95m", # Magenta (Concentrador)
    "\033[94m", # Azul
    "\033[91m", # Rojo
]
colors_map = {}
for i, port in enumerate(ports):
    colors_map[port] = colors[i % len(colors)]

reset_color = "\033[0m"

def monitor_port(port):
    color = colors_map.get(port, "")
    while True:
        try:
            print(f"[{datetime.now().strftime('%H:%M:%S')}] Intentando conectar a {port}...")
            ser = serial.Serial(port, baudrate, timeout=1)
            print(f"[{datetime.now().strftime('%H:%M:%S')}] {port} CONECTADO con éxito.")
            
            # Forzar reinicio de las placas para ver el proceso de arranque desde el inicio
            ser.dtr = False
            ser.rts = False
            time.sleep(0.1)
            ser.dtr = True
            ser.rts = True
            time.sleep(0.1)
            
            while True:
                line = ser.readline()
                if line:
                    try:
                        decoded = line.decode('utf-8', errors='ignore').strip()
                        if decoded:
                            timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
                            print(f"{color}[{timestamp}] [{port}] {decoded}{reset_color}")
                    except Exception as e:
                        pass
                else:
                    if not ser.is_open:
                        break
        except Exception as e:
            time.sleep(2)

print(f"[{datetime.now().strftime('%H:%M:%S')}] Iniciando monitoreo dinámico en los puertos: {ports}")
threads = []
for port in ports:
    t = threading.Thread(target=monitor_port, args=(port,), daemon=True)
    t.start()
    threads.append(t)

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("\nDeteniendo monitoreo de puertos serie.")
