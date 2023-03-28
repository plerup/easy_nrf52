#!/usr/bin/env python3
#====================================================================================
# Use an external nrf52840 with the program ble_tool loaded for Python BLE tools.
# This enables use of larger MTU and long range, something that is not available
# when using Bluez
#
# This file is part of easy_nrf52
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/easy_nrf52
#
# Copyright (c) 2023 Peter Lerup. All rights reserved.
#
#====================================================================================

import sys, time

from serial import Serial, SerialException
from serial.tools import list_ports
import argparse

connected = False
connecting = False
uart = None
args = None
indicator_led = 3

def err_mess(mess):
    print(f"* {mess}", file=sys.stderr)

#--------------------------------------------------------------------

def read_string():
    global connecting, connected
    start = time.time()
    try:
        resp = uart.readline().decode().strip()
    except Exception as e:
        raise SerialException

    if args.debug:
        print("In:", resp)
    if time.time()-start > uart.timeout:
        err_mess("Response timeout")
        raise EOFError
    if "#CONNECTED" in resp:
        connecting = False
        connected = True
    if "#DISCONNECTED" in resp:
        err_mess("Peripheral has disconnected")
        connected = False
        raise EOFError

    return resp


#--------------------------------------------------------------------

def in_waiting():
    try:
        return uart.in_waiting
    except Exception as e:
        raise SerialException


#--------------------------------------------------------------------

def send_string(str, wait_for=None, timeout=3, ignore_err=False):
    global connecting, connected
    if args.debug:
        print(f"Out: {str}")
    uart.write((str + "\n").encode())
    if not wait_for:
        wait_for = "="
    uart.timeout = timeout
    if "connect" in str.lower():
        connecting = True
    while True:
        resp = read_string()
        if wait_for in resp:
            return resp[len(wait_for):]
        if resp[0] == "*":
            if ignore_err:
                return
            else:
                print(resp)
                raise EOFError

#--------------------------------------------------------------------

def init(parser):
    global uart, args
    def_port = "/dev/ttyACM0"
    for port in list_ports.comports():
        if "nRF52 USB " in str(port):
            def_port = port.device

    parser.add_argument('-p', dest='port', default=def_port,
                        help='Serial port')
    parser.add_argument('-l', dest='long_range',
                        default=0, action='store_const', const=1,
                        help='Use long range BLE')
    parser.add_argument('-d', dest='debug', action='store_true',
                        help='Debug mode')
    args = parser.parse_args()

    try:
        uart = Serial(port=args.port, baudrate=115200, timeout=3)
        send_string("disconnect", ignore_err=True)
        send_string("scan;", ignore_err=True)
        send_string('vers')
        send_string(f"led;{indicator_led};1")
        ok = True

    except (EOFError):
        err_mess(f"No response on: {args.port}")
        ok = False
    except (SerialException):
        err_mess(f"Invalid port: {args.port}")
        ok = False
    finally:
        return args if ok else None

#--------------------------------------------------------------------

def run(cmd):
    try:
        cmd()

    except (KeyboardInterrupt, EOFError):
        try:
            send_string(f"led;{indicator_led};0")
            if connected:
                print("\nDisconnecting...")
                send_string("disconnect")
                time.sleep(0.5)
            elif connecting:
                send_string("cancel_connect", ignore_err=True)
        except Exception as e:
            sys.exit(1)

    except (SerialException):
        err_mess("Serial port error")
