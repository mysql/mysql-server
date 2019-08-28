# -*- cperl -*-
# Copyright (c) 2004 MySQL AB, 2008 Sun Microsystems, Inc.
# Use is subject to license terms.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

# These are not to be prefixed with "mtr_"

sub gprof_prepare ();
sub gprof_collect ();

##############################################################################
#
#  
#
##############################################################################

sub gprof_prepare () {

  rmtree($::opt_gprof_dir);
  mkdir($::opt_gprof_dir);
}

# FIXME what about master1 and slave1?!
sub gprof_collect () {

  if ( -f "$::master->[0]->{'path_myddir'}/gmon.out" )
  {
    # FIXME check result code?!
    mtr_run("gprof",
            [$::exe_master_mysqld,
             "$::master->[0]->{'path_myddir'}/gmon.out"],
            $::opt_gprof_master, "", "", "");
    print "Master execution profile has been saved in $::opt_gprof_master\n";
  }
  if ( -f "$::slave->[0]->{'path_myddir'}/gmon.out" )
  {
    # FIXME check result code?!
    mtr_run("gprof",
            [$::exe_slave_mysqld,
             "$::slave->[0]->{'path_myddir'}/gmon.out"],
            $::opt_gprof_slave, "", "", "");
    print "Slave execution profile has been saved in $::opt_gprof_slave\n";
  }
}


1;
