#!/usr/bin/env perl
#====================================================================================
# Check for possible changed defines on make command line
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

sub read_file {
  my $f;
  open($f, $_[0]);
  my $res = <$f>;
  close($f);
  return $res;
}

sub write_file {
  my $f;
  open($f, ">$_[0]");
  print $f $_[1];
  close($f);
}

my $pid = shift;
my $dir = shift;
my $state_file = "$dir/_cmd_defines";
my $prev_defs = read_file($state_file);
my $curr_defs;
print STDERR read_file("/proc/$pid/cmdline"),"\n";
foreach (split(/\0+/,read_file("/proc/$pid/cmdline"))) {
  $curr_defs .= "$_ " if /\=/;
}
$curr_defs .= join(" ", @ARGV);
$curr_defs =~ s/IGNORE_STATE=1//;
my $diff = $curr_defs && $prev_defs && $curr_defs ne $prev_defs;
print STDERR "$curr_defs -- $prev_defs -- $diff\n";
write_file($state_file, $curr_defs);
my $diff = $curr_defs && $prev_defs && $curr_defs ne $prev_defs;
print "Changed" if $diff;
print STDERR "$curr_defs -- $prev_defs -- $diff\n";
