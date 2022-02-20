#!/usr/bin/env perl
#====================================================================================
#
# Parse UICR memory dump and show set values
#
# This file is part of easy_nrf52
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/easy_nrf52
#
# Copyright (c) 2022 Peter Lerup. All rights reserved.
#
#====================================================================================

print "== NRF UICR registers\n";
while (<>) {
  next unless s/^0x(\w+):\s+//;
  my $addr = hex($1);
  foreach my $val (split(/\s+/)) {
    last if $val =~ /\|/;
    print sprintf("%04X: %s\n", $addr & 0x0FFF, uc($val)) unless $val =~ /ffffffff/i;
    $addr += 4
  }
}
print "\nInfo at: https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Fuicr.html\n"
