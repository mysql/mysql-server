#!/usr/bin/perl -w -*- perl -*-
# Copyright (C) 2000, 2003, 2005 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# Generate text files from top directory from the manual.

$from = shift(@ARGV);
$fnode = shift(@ARGV);
$tnode = shift(@ARGV);

open(IN, "$from") || die "Cannot open $from: $!";

$in = 0;

while (<IN>)
{
  if ($in)
  {
    if (/Node: $tnode,/ || /\[index/)
    {
      $in = 0;
    }
    elsif (/^File: mysql.info/ || (/^/))
    {
      # Just Skip node beginnings
    }
    else
    {
      print;
    }
  }
  else
  {
    if (/Node: $fnode,/)
    {
      $in = 1;
      # Skip first empty line
      <IN>;
    }
  }
}

close(IN);

die "Could not find node \"$tnode\"" if ($in == 1);
exit 0;
