# -*- cperl -*-

# Copyright (c) 2007, 2017, Oracle and/or its affiliates. All rights reserved.
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
			     ["share/mysql", "share"],
			     "charsets");
print "charset_dir: $charset_dir\n";
print "=" x 40, "\n";
