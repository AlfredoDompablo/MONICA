import serial
import threading
import sys
import time
from datetime import datetime

ports = ["/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyACM2", "/dev/ttyACM3"]
baudrate = 115200

colors = {
    "/dev/ttyACM0": "\033[93m", # Yellow (Node 1)
    "/dev/ttyACM1": "\033[96m", # Cyan (Node 2)
    "/dev/ttyACM2": "\033[92m", # Green (Node 3 / 4)
    "/dev/ttyACM3": "\033[95m", # Magenta (Concentrador / Node 0)
}
reset_color = "\033[0m"

def monitor_port(port):
    color = colors.get(port, "")
    while True:
        try:
            print(f"[{datetime.now().strftime('%H:%M:%S')}] Intentando conectar a {port}...")
            ser = serial.Serial(port, baudrate, timeout=1)
            print(f"[{datetime.now().strftime('%H:%M:%S')}] {port} CONECTADO con éxito.")
            
            # Reset chip by toggling DTR and RTS (some boards need this to reboot cleanly)
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
                    # check if connection is still alive
                    if not ser.is_open:
                        break
        except Exception as e:
            # Print reconnection error periodically
            time.sleep(2)

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
