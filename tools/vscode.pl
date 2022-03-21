#!/usr/bin/env perl
#====================================================================================
# vscode.pl
#
# Generates Visual Studio Code properties, task and launch config files
# based on compile command line content and then starts VS Code
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
use Cwd;
use JSON::PP;
use Getopt::Std;
use File::Basename;

my $workspace_dir;

sub file_to_string {
  local($/);
  my $f;
  open($f, $_[0]) || return "";
  my $res = <$f>;
  close($f);
  return $res;
}

#--------------------------------------------------------------------

sub string_to_file {
  my $f;
  open($f, ">$_[0]") || return 0;
  print $f $_[1];
  close($f);
  return 1;
}

#--------------------------------------------------------------------

sub find_dir_upwards {
  my $match = $_[0];
  my $dir = Cwd::abs_path(".");
  while ($dir ne "/" && $dir ne $ENV{'HOME'}) {
    my $test = $dir . "/$match";
    return $test if -e $test;
    $dir = dirname($dir);
  }
  return Cwd::abs_path(".") . "/$match";
}

#--------------------------------------------------------------------

sub insert_or_replace {
  my (
    $file_name,
    $def_json,
    $item_json,
    $list_name,
    $match,
    $keep
  ) = @_;
  $item_json =~ s/$ENV{'HOME'}/\$\{env:HOME\}/g;
  my $data = decode_json(file_to_string($file_name) || $def_json);
  my $new_item = decode_json($item_json);
  # Find possible existing item
  my $list_ref = $$data{$list_name};
  my $ind = 0;
  foreach my $item (@$list_ref) {
    if ($$item{$match} eq $$new_item{$match}) {
      $$new_item{$keep} = $$item{$keep} if $keep;
      last;
    }
    $ind++;
  }
  # Update or insert the item
  $$list_ref[$ind] = $new_item;
  string_to_file($file_name, JSON::PP->new->pretty->encode(\%$data));
}

#--------------------------------------------------------------------

# Parameters
my %opts;
getopts('n:m:w:d:i:p:o:b:', \%opts);
my $name = $opts{n} || "Linux";
my $make_com = $opts{m} || "espmake";
$workspace_dir = $opts{w};
my $proj_file = $opts{p};
my $cwd = $opts{d} || getcwd;
my $comp_path = shift;
$comp_path = shift if $comp_path eq "ccache";
my $toolchain_path = dirname($comp_path) . "/";
my $exec_path = $opts{o};
my $dbg_int = $opts{b};

my $config_dir_name = ".vscode";
$workspace_dir ||= dirname(find_dir_upwards($config_dir_name));
$proj_file ||= (glob("$workspace_dir/*.code-workspace"))[0];
my $config_dir = "$workspace_dir/$config_dir_name";
mkdir($config_dir);

# == C & C++ configuration
my @defines;
my @includes;
# Build this configuration from command line defines and includes
while ($_ = shift) {
  $_ .= shift if ($_ eq "-D");
  if (/-D\s*(\S+)/) {
    # May be a quoted value
    my $def = $1;
    $def =~ s/\"/\\\"/g;
    push(@defines, "\"$def\"")
  }
  push(@includes, "\"" . Cwd::abs_path($1) . "\"") if /-I\s*(\S+)/ && -e $1;
}
# Optional additional include directories
foreach (split(" ", $opts{i})) {
  push(@includes, "\"$_\"");
}

# Build corresponding json
my $def = join(',', @defines);
my $inc = join(',', @includes);
my $json = <<"EOT";
{
  "name": "$name",
  "includePath": [$inc],
  "defines": [$def],
  "compilerPath": "$comp_path",
  "cStandard": "gnu99",
  "cppStandard": "gnu++11"
}
EOT
insert_or_replace("$config_dir/c_cpp_properties.json", '{"version": 4, "configurations": []}',
                  $json, 'configurations', 'name');

# == Add build tasks
$json = <<"EOT";
{
  "label": "$name",
  "type": "shell",
  "command": "$make_com",
  "options": {"cwd": "$cwd"},
  "problemMatcher": ["\$gcc"],
  "group": "build"
}
EOT
# Normal build task
insert_or_replace("$config_dir/tasks.json", '{"version": "2.0.0", "tasks": []}',
                  $json, 'tasks', 'label', 'group');
# Debug task
$json =~ s/(\"$name)/${1}_debug/;
my $debug_op = "DEBUG=1";
$debug_op = "" if $make_com =~ /$debug_op/;
$json =~ s/(\"$make_com)/${1} $debug_op build_flash/;
insert_or_replace("$config_dir/tasks.json", '{"version": "2.0.0", "tasks": []}',
                  $json, 'tasks', 'label', 'group');


# == Add debug launcher
my $json = <<"EOT";
{
  "name": "$name",
  "cwd": "$cwd",
  "executable": "$exec_path",
  "armToolchainPath": "$toolchain_path",
  "request": "launch",
  "type": "cortex-debug",
  "runToEntryPoint": "main",
  "servertype": "$dbg_int",
  "configFiles": [
    "interface/$dbg_int.cfg",
    "nrf52.cfg"
  ],
  "device": "nrf52",
  "interface": "swd",
  "preLaunchTask": "${name}_debug"
}
EOT
insert_or_replace("$config_dir/launch.json", '{"version": "0.2.0", "configurations": []}',
                  $json, 'configurations', 'name');


# Launch Visual Studio Code
$proj_file ||= $workspace_dir;
print "Starting VS Code - $proj_file ...\n";
# Remove all MAKE variables to avoid conflict when building inside VS Code
foreach my $var (keys %ENV) {
  $ENV{$var} = undef if $var =~ /^MAKE/;
}
system("code $proj_file");
