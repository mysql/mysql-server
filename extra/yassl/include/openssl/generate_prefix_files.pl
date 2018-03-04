#!/usr/bin/perl

# Copyright (C) 2006 MySQL AB
# Use is subject to license terms
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#
# This script generates defines for all functions
# in yassl/include/openssl/ so they are renamed to
# ya<old_function_name>. Hopefully that is unique enough.
#
# The script is to be run manually when we import
# a new version of yaSSL
#



# Find all functions in "input" and add macros
# to prefix/rename them into "output
sub generate_prefix($$)
{
  my $input= shift;
  my $output= shift;
  open(IN, $input)
      or die("Can't open input file $input: $!");
  open(OUT, ">", $output)
    or mtr_error("Can't open output file $output: $!");

  while (<IN>)
  {
    chomp;

    if ( /typedef/ )
    {
      next;
    }

    if ( /^\s*[a-zA-Z0-9*_ ]+\s+\*?([_a-zA-Z0-9]+)\s*\(/ )
    {
      print OUT "#define $1 ya$1\n";
    }
  }

  close OUT;
  close IN;
}

generate_prefix("ssl.h", "prefix_ssl.h");
generate_prefix("crypto.h", "prefix_crypto.h");

