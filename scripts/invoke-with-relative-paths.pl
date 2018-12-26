#! /usr/bin/perl
#
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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
# along with this program; if not, write to the Free Software Foundation,
# 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA
#

#
# Take the given GCC command line and run it with all absolute paths
# changed to relative paths. This makes sure that no part of the build
# path leaks into the .o files, which it normally would through the
# contents of __FILE__. (Debug information is also affected, but that
# is already fixed through -fdebug-prefix-map=.)
#
# A more elegant solution would be -ffile-prefix-map=, but this is
# not currently supported in GCC; see
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70268.
#

use strict;
use warnings;
use Cwd;

my $cwd = getcwd();

my @newarg = ();
for my $i (0..$#ARGV) {
	my $arg = $ARGV[$i];
	if ($arg =~ /-I(.+)$/) {
		$arg = '-I' . relativize($1, $cwd);
	} elsif ($arg =~ /^\//) {
		$arg = relativize($arg, $cwd);
	}
	push @newarg, $arg;
}

exec(@newarg);

# /a/b/c/foo from /a/b/d = ../c/foo
sub relativize {
	my ($dir1, $dir2) = @_;

	if ($dir1 !~ /^\//) {
		# Not an absolute path.
		return $dir1;
	}

	if (! -e $dir1) {
#		print STDERR "Unknown file/directory $dir1.\n";
		return $dir1;
	}
	# Resolve symlinks and such, because getcwd() does.
	$dir1 = Cwd::abs_path($dir1);

	if ($dir1 =~ /^\/(lib|tmp|usr)/) {
		# Not related to our source code.
		return $dir1;
	}

	if ($dir1 eq $dir2) {
		return ".";
	}

	my (@dir1_components) = split /\//, $dir1;
	my (@dir2_components) = split /\//, $dir2;

	# Remove common leading components.
	while (scalar @dir1_components > 0 && scalar @dir2_components > 0 &&
	       $dir1_components[0] eq $dir2_components[0]) {
		shift @dir1_components;
		shift @dir2_components;
	}

	my $ret = "";
	for my $i (0..$#dir2_components) {
		$ret .= '../';
	}
	$ret .= join('/', @dir1_components);

	# print STDERR "[$dir1] from [$dir2] => [$ret]\n";

	return $ret;
}

