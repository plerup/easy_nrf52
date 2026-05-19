
#!/usr/bin/env python3
#====================================================================================
# Segger RTT monitor
#
# This file is part of easy_nrf52
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/easy_nrf52
#
# Copyright (c) 2026 Peter Lerup. All rights reserved.
#
#====================================================================================

from time import sleep
from pynrfjprog import LowLevel
from re import match
from argparse import ArgumentParser

JLINK_SPEED_KHZ = 50000
READ_SIZE = 1024
COM_CHANNEL_ID = 0

RED     = "\033[1;31m"
GREEN   = "\033[1;32m"
ORANGE  = "\033[1;33m"
BLUE    = "\033[1;34m"
END_COL = "\033[0m"

def read_callback(channel_index, data, _):
    data = data.decode()
    for line in data.splitlines():
        if line:
            col = GREEN
            m = match(r"^(<\w+>\s+[^:]+:\s+)(.+)", line)
            if m:
                if "error" in m.group(1):
                    col = RED
                elif "warning" in m.group(1):
                    col = ORANGE
                elif "debug" in m.group(1):
                    col = BLUE
                print(m.group(1) + col + m.group(2) + END_COL, flush=True)
            else:
                print(line, flush=True)

#--------------------------------------------------------------------

def fail(mess):
    print(f"{RED}* {mess}{END_COL}")
    exit(1)

#--------------------------------------------------------------------

def rtt_monitor(args):
    api = LowLevel.API()
    api.open()
    print("--- Connecting RTT...")
    try:
        if args.snr:
            api.connect_to_emu_with_snr(args.snr, jlink_speed_khz=JLINK_SPEED_KHZ)
        else:
            api.connect_to_emu_without_snr(jlink_speed_khz=JLINK_SPEED_KHZ)
    except:
        fail("Failed to connect to JLink device")

    api.rtt_start()
    tries = 0
    while not api.rtt_is_control_block_found():
        sleep(1)
        tries += 1
        if tries > 5:
            print("Restarting the device")
            api.sys_reset()
            api.go()
            tries = 0

    print("- Connected. Enter command or ctrl-C to exit")
    api.rtt_async_callback_start(COM_CHANNEL_ID, READ_SIZE, read_callback)
    try:
        while True:
            try:
                cmd = input()
                if cmd:
                    api.rtt_write(COM_CHANNEL_ID, cmd)
                else:
                   raise EOFError
            except EOFError:
                print(f"{RED}Use ctrl-C to exit{END_COL}")
    except KeyboardInterrupt:
        print()

    print("--- Disconnecting RTT...")
    api.close()

if __name__ == "__main__":
    parser = ArgumentParser(description="RTT terminal")
    parser.add_argument("--snr", type=int, nargs="?", default=None, help="Optional Segger serial number")
    args = parser.parse_args()
    rtt_monitor(args)


