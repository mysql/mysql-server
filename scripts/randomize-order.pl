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
# Randomize each .text and .data name (unless it is already randomized),
# so that we get a different order of all the symbols in the final binary
# (assuming -ffunction-sections -fdata-sections and -Wl,--sort-section=name).
# This makes it possible to see if a performance change is due to
# semirandom address positioning effects or a genuine change that will be
# robust to other, unrelated changes.
#
# Usage: randomize-order.pl SEED [OBJFILES...]

use strict;
use warnings;
use Digest::SHA;

my $seed = shift @ARGV;

for my $objfile (@ARGV) {
	my @objcopy_args = ();

	# Read in the list of sections, and find all that start with .text or .data.
	open my $objdumpfh, "-|", "objdump", "-h", $objfile
		or die "objdump: $!";
	while (<$objdumpfh>) {
		chomp;
		if (/^\s*\d+\s*(\.text|\.data)\.(\S+)\s*/) {
			my ($section_type, $section_name) = ($1, $2);

			# Leave already reordered sections alone.
			next if ($section_name =~ /\.__random_order$/);

			my $new_name = Digest::SHA::sha256_hex($seed . $section_name) . ".__random_order";
			push @objcopy_args, "--rename=$section_type.$section_name=$section_type.$new_name";
		}
	}
	close $objdumpfh;

	while (scalar @objcopy_args > 0) {
		# Some targets are too big for one objcopy run in debug mode
		# (the command line gets too long); split into smaller chunks.
		my @args;
		if (scalar @objcopy_args >= 4096) {
			@args = @objcopy_args[0..4095];
			@objcopy_args = @objcopy_args[4096..$#objcopy_args];
		} else {
			@args = @objcopy_args;
			@objcopy_args = ();
		}

		system("objcopy", @args, $objfile, "$objfile.tmp") == 0
			or die "objcopy: $?";
		rename("$objfile.tmp", $objfile)
			or die "rename($objfile): $!";
	}
}
