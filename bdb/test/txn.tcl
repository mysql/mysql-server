# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: txn.tcl,v 11.12 2000/12/31 19:26:23 bostic Exp $
#
# Options are:
# -dir <directory in which to store memp>
# -max <max number of concurrent transactions>
# -iterations <iterations>
# -stat
proc txn_usage {} {
	puts "txn -dir <directory> -iterations <number of ops> \
	    -max <max number of transactions> -stat"
}

proc txntest { args } {
	source ./include.tcl

	# Set defaults
	set iterations 50
	set max 1024
	set dostat 0
	set flags ""
	for { set i 0 } { $i < [llength $args] } {incr i} {
		switch -regexp -- [lindex $args $i] {
			-d.* { incr i; set testdir [lindex $args $i] }
			-f.* { incr i; set flags [lindex $args $i] }
			-i.* { incr i; set iterations [lindex $args $i] }
			-m.* { incr i; set max [lindex $args $i] }
			-s.* { set dostat 1 }
			default {
				puts -nonewline "FAIL:[timestamp] Usage: "
				txn_usage
				return
			}
		}
	}
	if { $max < $iterations } {
		set max $iterations
	}

	# Now run the various functionality tests
	txn001 $testdir $max $iterations $flags
	txn002 $testdir $max $iterations
}

proc txn001 { dir max ntxns flags} {
	source ./include.tcl

	puts "Txn001: Basic begin, commit, abort"

	# Open environment
	env_cleanup $dir

	set env [eval {berkdb \
	    env -create -mode 0644 -txn -txn_max $max -home $dir} $flags]
	error_check_good evn_open [is_valid_env $env] TRUE
	txn001_suba $ntxns $env
	txn001_subb $ntxns $env
	txn001_subc $ntxns $env
	# Close and unlink the file
	error_check_good env_close:$env [$env close] 0
}

proc txn001_suba { ntxns env } {
	source ./include.tcl

	# We will create a bunch of transactions and commit them.
	set txn_list {}
	set tid_list {}
	puts "Txn001.a: Beginning/Committing $ntxns Transactions in $env"
	for { set i 0 } { $i < $ntxns } { incr i } {
		set txn [$env txn]
		error_check_good txn_begin [is_valid_txn $txn $env] TRUE

		lappend txn_list $txn

		set tid [$txn id]
		error_check_good tid_check [lsearch $tid_list $tid] -1

		lappend tid_list $tid
	}

	# Now commit them all
	foreach t $txn_list {
		error_check_good txn_commit:$t [$t commit] 0
	}
}

proc txn001_subb { ntxns env } {
	# We will create a bunch of transactions and abort them.
	set txn_list {}
	set tid_list {}
	puts "Txn001.b: Beginning/Aborting Transactions"
	for { set i 0 } { $i < $ntxns } { incr i } {
		set txn [$env txn]
		error_check_good txn_begin [is_valid_txn $txn $env] TRUE

		lappend txn_list $txn

		set tid [$txn id]
		error_check_good tid_check [lsearch $tid_list $tid] -1

		lappend tid_list $tid
	}

	# Now abort them all
	foreach t $txn_list {
		error_check_good txn_abort:$t [$t abort] 0
	}
}

proc txn001_subc { ntxns env } {
	# We will create a bunch of transactions and commit them.
	set txn_list {}
	set tid_list {}
	puts "Txn001.c: Beginning/Prepare/Committing Transactions"
	for { set i 0 } { $i < $ntxns } { incr i } {
		set txn [$env txn]
		error_check_good txn_begin [is_valid_txn $txn $env] TRUE

		lappend txn_list $txn

		set tid [$txn id]
		error_check_good tid_check [lsearch $tid_list $tid] -1

		lappend tid_list $tid
	}

	# Now prepare them all
	foreach t $txn_list {
		error_check_good txn_prepare:$t [$t prepare] 0
	}

	# Now commit them all
	foreach t $txn_list {
		error_check_good txn_commit:$t [$t commit] 0
	}

}

# Verify that read-only transactions do not create any log records
proc txn002 { dir max ntxns } {
	source ./include.tcl

	puts "Txn002: Read-only transaction test"

	env_cleanup $dir
	set env [berkdb \
	    env -create -mode 0644 -txn -txn_max $max -home $dir]
	error_check_good dbenv [is_valid_env $env] TRUE

	# We will create a bunch of transactions and commit them.
	set txn_list {}
	set tid_list {}
	puts "Txn002.a: Beginning/Committing Transactions"
	for { set i 0 } { $i < $ntxns } { incr i } {
		set txn [$env txn]
		error_check_good txn_begin [is_valid_txn $txn $env] TRUE

		lappend txn_list $txn

		set tid [$txn id]
		error_check_good tid_check [lsearch $tid_list $tid] -1

		lappend tid_list $tid
	}

	# Now commit them all
	foreach t $txn_list {
		error_check_good txn_commit:$t [$t commit] 0
	}

	# Now verify that there aren't any log records.
	set r [$env log_get -first]
	error_check_good log_get:$r [llength $r] 0

	error_check_good env_close:$r [$env close] 0
}
