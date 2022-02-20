#!/usr/bin/env perl
#====================================================================================
# Find and merge all files and build options in
# a list of makefiles
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

my %paths;
my $cnt = 0;
my $output;

foreach my $makefile (@ARGV) {
  open(my $f, $makefile) || die "Failed to open the makefile at: $makefile\n";
  my $found;
  while (<$f>) {
    last if /^.PHONY/ || (/^LIB_FILES/ && $cnt);
    $found ||= /^SRC_FILES/;
    next unless $found;
    next if /^#/;
    s/\w+:\s+//;
    if (/(\S+)\s+\+=\s*(.+)/) {
      my ($id, $val) = ($1, $2);
      # Concatenate continuation lines
      while ($val =~ s/\\\s$/ /) {
        $val .= <$f>;
      }
      # Skip certain files and options
      $val =~ s/\$\(PROJ_DIR\)\S+//g;
      $val =~ s/\.\.\S+//g;
      $val =~ s/\n//g;
      next if $id =~ /^#/ || $val =~ /-DBOARD/;
      if ($id =~ /SRC_FILES|INC_FOLDERS/) {
        # Put path related variables in a hash to handle duplicates
        foreach (split(/\s+/, $val)) {
          next unless /\S/;
          $paths{$id}{$_}++;
        }
        next;
      } elsif ($id =~ /_FILES/) {
        $output .= "$id += $val\n";
        next;
      }
    }
    $output .= $_;
  }
  $cnt++;
  close($f);
}
# Show result
foreach my $id (keys %paths) {
  print "$id += \\\n";
  foreach my $val (sort keys %{$paths{$id}}) {
    print "  $val \\\n";
  }
  print "\n";
}

print $output;


