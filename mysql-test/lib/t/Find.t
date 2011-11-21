# -*- cperl -*-

# Copyright (c) 2007 MySQL AB
# Use is subject to license terms.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

use Test::More qw(no_plan);
use strict;

use_ok ("My::Find");
my $basedir= "../..";

print "=" x 40, "\n";
my $mysqld_exe= my_find_bin($basedir,
			    ["sql", "bin"],
                            ["mysqld", "mysqld-debug"]);
print "mysqld_exe: $mysqld_exe\n";
print "=" x 40, "\n";
my $mysql_exe= my_find_bin($basedir,
			   ["client", "bin"],
                           "mysql");
print "mysql_exe: $mysql_exe\n";
print "=" x 40, "\n";

my $mtr_build_dir= $ENV{MTR_BUILD_DIR};
$ENV{MTR_BUILD_DIR}= "debug";
my $mysql_exe= my_find_bin($basedir,
			   ["client", "bin"],
                           "mysql");
print "mysql_exe: $mysql_exe\n";
$ENV{MTR_BUILD_DIR}= $mtr_build_dir;
print "=" x 40, "\n";

my $charset_dir= my_find_dir($basedir,
			     ["share/mysql", "sql/share", "share"],
			     "charsets");
print "charset_dir: $charset_dir\n";
print "=" x 40, "\n";
