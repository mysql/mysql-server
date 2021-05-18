# -*- cperl -*-
# Copyright (c) 2004, 2018, Oracle and/or its affiliates. All rights reserved.
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

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

sub gprof_collect ($@) {
  my ($exe_mysqld, @gprof_dirs) = @_;

  print("Collecting gprof reports.....\n");

  foreach my $datadir (@gprof_dirs) {
    my $gprof_msg = "$datadir/gprof.msg";
    my $gprof_err = "$datadir/gprof.err";
    if (-f "$datadir/gmon.out") {
      system("gprof $exe_mysqld $datadir/gmon.out 2 > $gprof_err > $gprof_msg");
      print("GPROF output in $gprof_msg, errors in $gprof_err\n");
    }
  }
}

1;
