#!/usr/bin/env python3

from sys import argv, exit, stderr
from time import sleep
from serial.tools import list_ports

match = argv[1] if len(argv) > 1 else "ACM"
first = True
try:
    while True:
        for port in list_ports.comports():
            if match in str(port):
                print(port[0])
                exit(0)
        if first:
            print(f"Waiting for serial port matching: '{match}'...", file=stderr)
            first = False
        sleep(1)
except KeyboardInterrupt:
    exit(1)
