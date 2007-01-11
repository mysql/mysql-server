# -*- cperl -*-
# Copyright (C) 2004, 2006 MySQL AB
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

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

# These are not to be prefixed with "mtr_"

sub gcov_prepare ();
sub gcov_collect ();

##############################################################################
#
#  
#
##############################################################################

sub gcov_prepare () {

  `find $::glob_basedir -name \*.gcov \
    -or -name \*.da | xargs rm`;
}

# Used by gcov
our @mysqld_src_dirs=
  (
   "strings",
   "mysys",
   "include",
   "extra",
   "regex",
   "isam",
   "merge",
   "myisam",
   "myisammrg",
   "heap",
   "sql",
  );

sub gcov_collect () {

  print "Collecting source coverage info...\n";
  -f $::opt_gcov_msg and unlink($::opt_gcov_msg);
  -f $::opt_gcov_err and unlink($::opt_gcov_err);
  foreach my $d ( @mysqld_src_dirs )
  {
    chdir("$::glob_basedir/$d");
    foreach my $f ( (glob("*.h"), glob("*.cc"), glob("*.c")) )
    {
      `$::opt_gcov $f 2>>$::opt_gcov_err  >>$::opt_gcov_msg`;
    }
    chdir($::glob_mysql_test_dir);
  }
  print "gcov info in $::opt_gcov_msg, errors in $::opt_gcov_err\n";
}


1;
