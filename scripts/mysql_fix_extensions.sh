#!/usr/bin/perl

# Copyright (c) 2001, 2014, Oracle and/or its affiliates. All rights reserved.
# Use is subject to license terms.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
# MA 02110-1301, USA

# This is a utility for MySQL. It is not needed by any standard part
# of MySQL.

# Usage: mysql_fix_extentions datadir
# does not work with RAID, with InnoDB or BDB tables
# makes .frm lowercase and .MYI/MYD/ISM/ISD uppercase
# useful when datafiles are copied from windows

print STDERR "Warning: $0 is deprecated and will be removed in a future version.\n";
die "Usage: $0 datadir\n" unless -d $ARGV[0];

for $a (<$ARGV[0]/*/*.*>) { $_=$a;
  s/\.frm$/.frm/i;
  s/\.(is[md]|my[id])$/\U$&/i;
  rename ($a, $_) || warn "Cannot rename $a => $_ : $!";
}
