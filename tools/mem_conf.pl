#!/usr/bin/env perl
#====================================================================================
# Adjust RAM settings for linker script
#
# This file is part of easy_nrf52
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/easy_nrf52
#
# Copyright (c) 2022-2024 Peter Lerup. All rights reserved.
#
#====================================================================================

use strict;

my $offset = 0x20000000;
my $ram_reduc = hex(shift);
open(my $f, shift) || die "Failed to open linker file\n";
while (<$f>) {
  if (/^\s+RAM\s+.+ORIGIN\s*=\s*(\S+),\s*LENGTH\s*=\s*(\S+)/) {
    my $ram_size = hex($1)+hex($2)-$offset;
    print sprintf("  RAM (rwx) :  ORIGIN = 0x%X, LENGTH = 0x%X\n", $offset+$ram_reduc, $ram_size-$ram_reduc);
  } else {
    print;
  }
}
close($f);

