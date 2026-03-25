#!/usr/bin/env python3
#====================================================================================
# Patched version of Python serial miniterm
# Enables patching of the output line and
# avoids stack trace and port hanging after serial error
#
# This file is part of easy_nrf52
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/easy_nrf52
#
# Copyright (c) 2026 Peter Lerup. All rights reserved.
#
#====================================================================================

import serial
import os
from serial.tools import miniterm

try:
    # Possible external patch function
    from miniterm_patch import patch_line
except ImportError:
    def patch_line(line: str):
        return line

def reader(self):
    line_buffer = ""
    try:
        while self.alive:
            data = self.serial.read(self.serial.in_waiting or 1)
            if not data:
                continue
            try:
                text = data.decode(self.rx_encoding, errors="replace")
            except Exception:
                text = data.decode(errors="replace")
            line_buffer += text
            while "\n" in line_buffer:
                line, line_buffer = line_buffer.split("\n", 1)
                # User hook
                line = patch_line(line)
                self.console.write(line + "\n")

    except serial.SerialException:
        # Silent raw exit
        self.console.write("\033[1;31m* Serial error. Exit\033[0m\n")
        self.console.cleanup()
        os._exit(0)

#------------------------------------------------------------------------------

# Apply monkey-patch
miniterm.Miniterm.reader = reader

miniterm.main()
