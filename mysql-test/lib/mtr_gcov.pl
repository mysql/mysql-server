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

sub gcov_prepare ($) {
  my ($dir)= @_;

  `find $dir -name \*.gcov \
    -or -name \*.da | xargs rm`;
}

my @mysqld_src_dirs=
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

sub gcov_collect ($$$) {
  my ($dir, $gcov, $gcov_msg, $gcov_err)= @_;

  my $start_dir= cwd();

  print "Collecting source coverage info...\n";
  -f $gcov_msg and unlink($gcov_msg);
  -f $gcov_err and unlink($gcov_err);
  foreach my $d ( @mysqld_src_dirs )
  {
    chdir("$dir/$d");
    foreach my $f ( (glob("*.h"), glob("*.cc"), glob("*.c")) )
    {
      `$gcov $f 2>>$gcov_err  >>$gcov_msg`;
    }
    chdir($start_dir);
  }
  print "gcov info in $gcov_msg, errors in $gcov_err\n";
}


1;
