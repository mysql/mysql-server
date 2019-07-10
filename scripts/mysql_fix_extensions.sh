#!@PERL_PATH@

# Copyright (c) 2001, 2017, Oracle and/or its affiliates. All rights reserved.
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
