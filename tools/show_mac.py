#!/usr/bin/env python3
#====================================================================================
# Convert the extracted memory values of the ROM-based
# mac address of a nrf52 device into a readable format
#
# This file is part of easy_nrf52
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/easy_nrf52
#
# Copyright (c) 2023-2024 Peter Lerup. All rights reserved.
#
#====================================================================================

import sys, re

for line in sys.stdin:
    m = re.search(r"^0x\w+:\s+(\S+)\s+(\S+)", line)
    if m:
       print(re.sub(r"(\w\w)", r"\1:", "%12X" % (((int(m.group(2)[4:], 16) | 0xC000) << 32) + int(m.group(1), 16)))[:-1])
