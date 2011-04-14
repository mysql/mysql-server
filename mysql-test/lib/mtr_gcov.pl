# -*- cperl -*-
# Copyright (c) 2004, 2011, Oracle and/or its affiliates. All rights reserved.
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
  print "Purging gcov information from '$dir'...\n";

  system("find $dir -name \*.gcov -o -name \*.da"
             . " -o -name \*.gcda | grep -v 'README.gcov\$' | xargs rm");
}

#
# Collect gcov statistics.
# Arguments:
#   $dir       basedir, normally build directory
#   $gcov      gcov utility program [path] name
#   $gcov_msg  message file name
#   $gcov_err  error file name
#
sub gcov_collect ($$$) {
  my ($dir, $gcov, $gcov_msg, $gcov_err)= @_;

  # Get current directory to return to later.
  my $start_dir= cwd();

  print "Collecting source coverage info using '$gcov'...\n";
  -f "$dir/$gcov_msg" and unlink("$dir/$gcov_msg");
  -f "$dir/$gcov_err" and unlink("$dir/$gcov_err");

  my @dirs= `find "$dir" -type d -print | sort`;
  #print "List of directories:\n@dirs\n";

  foreach my $d ( @dirs ) {
    chomp($d);
    chdir($d) or next;

    my @flist= glob("*.*.gcno");
    print ("Collecting in '$d'...\n") if @flist;

    foreach my $f (@flist) {
      system("$gcov $f 2>>$dir/$gcov_err >>$dir/$gcov_msg");
    }
    chdir($start_dir);
  }
  print "gcov info in $dir/$gcov_msg, errors in $dir/$gcov_err\n";
}


1;
