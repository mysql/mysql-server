# -*- cperl -*-

# Copyright (c) 2008 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

use Test::More qw(no_plan);
use strict;

use_ok ("My::Platform");
use My::Platform;

use File::Temp qw / tempdir /;
my $dir = tempdir( CLEANUP => 1 );

print "Running on Windows\n" if (IS_WINDOWS);
print "Using ActiveState perl\n" if (IS_WIN32PERL);
print "Using cygwin perl\n" if (IS_CYGWIN);

print "dir: '$dir'\n";
print "native: '".native_path($dir)."'\n";
print "mixed: '".mixed_path($dir)."'\n";
print "posix: '".posix_path($dir)."'\n";
