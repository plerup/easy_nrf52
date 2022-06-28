#!/usr/bin/env python3
#
# show_mac.py
#
# Converts the extracted memory values of the ROM-based
# mac address of a nrf52 device into a readable format

import sys, re

for line in sys.stdin:
    m = re.search(r"^0x\w+:\s+(\S+)\s+(\S+)", line)
    if m:
       print(re.sub(r"(\w\w)", r"\1:", "%12X" % (((int(m.group(2)[4:], 16) | 0xC000) << 32) + int(m.group(1), 16)))[:-1])
