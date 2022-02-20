#!/usr/bin/env perl
#====================================================================================
# Fix possible UICR address error in bootloader
# linker memory configuration file
#
# This file is part of easy_nrf52
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/easy_nrf52
#
# Copyright (c) 2022 Peter Lerup. All rights reserved.
#
#====================================================================================

use strict;

open(my $f, shift) || die "Failed to open linker file\n";
while (<$f>) {
  s/(uicr_bootloader_start_address.+ ORIGIN = 0x)\w+/${1}10001014/;
  s/(uicr_mbr_params_page.+ ORIGIN = 0x)\w+/${1}10001018/;
  print $_;
}
close($f);

