# -*- cperl -*-

# Copyright (c) 2008 MySQL AB
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
