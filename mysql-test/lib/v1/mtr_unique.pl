# -*- cperl -*-
# Copyright (c) 2006 MySQL AB, 2008 Sun Microsystems, Inc.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#
# This file is used from mysql-test-run.pl when choosing
# port numbers and directories to use for running mysqld.
#

use strict;
use Fcntl ':flock';

#
# Requested IDs are stored in a hash and released upon END.
#
my %mtr_unique_assigned_ids = ();
my $mtr_unique_pid;
BEGIN {
	$mtr_unique_pid = $$ unless defined $mtr_unique_pid;
}
END { 
	if($mtr_unique_pid == $$) {
		while(my ($id,$file) = each(%mtr_unique_assigned_ids)) {
			print "Autoreleasing $file:$id\n";
			mtr_release_unique_id($file, $id);
		}
	}
}

#
# Require a unique, numerical ID, given a file name (where all
# requested IDs are stored), a minimum and a maximum value.
#
# We use flock to implement locking for the ID file and ignore
# possible problems arising from lack of support for it on 
# some platforms (it should work on most, and the possible
# race condition would occur rarely). The proper solution for
# this is a daemon that manages IDs, of course.
#
# If no unique ID within the specified parameters can be 
# obtained, return undef.
#
sub mtr_require_unique_id($$$) {
	my $file = shift;
	my $min = shift;
	my $max = shift;
	my $ret = undef;
	my $changed = 0;

	my $can_use_ps = `ps -e | grep '^[ ]*$$ '`;

	if(eval("readlink '$file'") || eval("readlink '$file.sem'")) {
		die 'lock file is a symbolic link';
	}

	chmod 0777, "$file.sem";
	open SEM, ">", "$file.sem" or die "can't write to $file.sem";
	flock SEM, LOCK_EX or die "can't lock $file.sem";
	if(! -e $file) {
		open FILE, ">", $file or die "can't create $file";
		close FILE;
	}

	if(eval("readlink '$file'") || eval("readlink '$file.sem'")) {
		die 'lock file is a symbolic link';
	}

	chmod 0777, $file;
	open FILE, "+<", $file or die "can't open $file";
	select undef,undef,undef,0.2;
	seek FILE, 0, 0;
	my %taken = ();
	while(<FILE>) {
		chomp;
		my ($id, $pid) = split / /;
		$taken{$id} = $pid;
		if($can_use_ps) {
			my $res = `ps -e | grep '^[ ]*$pid '`;
			if(!$res) {
				print "Ignoring slot $id used by missing process $pid.\n";
				delete $taken{$id};
				++$changed;
			}
		}
	}
	for(my $i=$min; $i<=$max; ++$i) {
		if(! exists $taken{$i}) {
			$ret = $i;
			$taken{$i} = $$;
			++$changed;
			last;
		}
	}
	if($changed) {
		seek FILE, 0, 0;
		truncate FILE, 0 or die "can't truncate $file";
		for my $k (keys %taken) {
			print FILE $k . ' ' . $taken{$k} . "\n";
		}
	}
	close FILE;
	flock SEM, LOCK_UN or warn "can't unlock $file.sem";
	close SEM;
	$mtr_unique_assigned_ids{$ret} = $file if defined $ret;
	return $ret;
}

#
# Require a unique ID like above, but sleep if no ID can be
# obtained immediately.
#
sub mtr_require_unique_id_and_wait($$$) {
	my $ret = mtr_require_unique_id($_[0],$_[1],$_[2]);
	while(! defined $ret) {
		sleep 30;
		$ret = mtr_require_unique_id($_[0],$_[1],$_[2]);
		print "Waiting for unique id to become available...\n" unless $ret;
	}
	return $ret;
}

#
# Release a unique ID.
#
sub mtr_release_unique_id($$) {
	my $file = shift;
	my $myid = shift;

	if(eval("readlink '$file'") || eval("readlink '$file.sem'")) {
		die 'lock file is a symbolic link';
	}

	open SEM, ">", "$file.sem" or die "can't write to $file.sem";
	flock SEM, LOCK_EX or die "can't lock $file.sem";

	if(eval("readlink '$file'") || eval("readlink '$file.sem'")) {
		die 'lock file is a symbolic link';
	}

	if(! -e $file) {
		open FILE, ">", $file or die "can't create $file";
		close FILE;
	}
	open FILE, "+<", $file or die "can't open $file";
	select undef,undef,undef,0.2;
	seek FILE, 0, 0;
	my %taken = ();
	while(<FILE>) {
		chomp;
		my ($id, $pid) = split / /;
		$taken{$id} = $pid;
	}
	delete $taken{$myid};
	seek FILE, 0, 0;
	truncate FILE, 0 or die "can't truncate $file";
	for my $k (keys %taken) {
		print FILE $k . ' ' . $taken{$k} . "\n";
	}
	close FILE;
	flock SEM, LOCK_UN or warn "can't unlock $file.sem";
	close SEM;
	delete $mtr_unique_assigned_ids{$myid};
}

1;

