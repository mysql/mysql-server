# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test042.tcl,v 11.24 2000/08/25 14:21:56 sue Exp $
#
# DB Test 42 {access method}
#
# Multiprocess DB test; verify that locking is working for the concurrent
# access method product.
#
# Use the first "nentries" words from the dictionary.  Insert each with self
# as key and a fixed, medium length data string.  Then fire off multiple
# processes that bang on the database.  Each one should try to read and write
# random keys.  When they rewrite, they'll append their pid to the data string
# (sometimes doing a rewrite sometimes doing a partial put).  Some will use
# cursors to traverse through a few keys before finding one to write.

set datastr abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz

proc test042 { method {nentries 1000} args } {
	global datastr
	source ./include.tcl

	#
	# If we are using an env, then skip this test.  It needs its own.
	set eindex [lsearch -exact $args "-env"]
	if { $eindex != -1 } {
		incr eindex
		set env [lindex $args $eindex]
		puts "Test042 skipping for env $env"
		return
	}
	set args [convert_args $method $args]
	set omethod [convert_method $method]

	puts "Test042: CDB Test $method $nentries"

	# Set initial parameters
	set do_exit 0
	set iter 10000
	set procs 5

	# Process arguments
	set oargs ""
	for { set i 0 } { $i < [llength $args] } {incr i} {
		switch -regexp -- [lindex $args $i] {
			-dir	{ incr i; set testdir [lindex $args $i] }
			-iter	{ incr i; set iter [lindex $args $i] }
			-procs	{ incr i; set procs [lindex $args $i] }
			-exit	{ set do_exit 1 }
			default { append oargs " " [lindex $args $i] }
		}
	}

	# Create the database and open the dictionary
	set testfile test042.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3

	env_cleanup $testdir

	set env [berkdb env -create -cdb -home $testdir]
	error_check_good dbenv [is_valid_widget $env env] TRUE

	set db [eval {berkdb_open -env $env -create -truncate \
	    -mode 0644 $omethod} $oargs {$testfile}]
	error_check_good dbopen [is_valid_widget $db db] TRUE

	set did [open $dict]

	set pflags ""
	set gflags ""
	set txn ""
	set count 0

	# Here is the loop where we put each key/data pair
	puts "\tTest042.a: put/get loop"
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [is_record_based $method] == 1 } {
			set key [expr $count + 1]
		} else {
			set key $str
		}
		set ret [eval {$db put} \
		    $txn $pflags {$key [chop_data $method $datastr]}]
		error_check_good put:$db $ret 0
		incr count
	}
	close $did
	error_check_good close:$db [$db close] 0

	# Database is created, now set up environment

	# Remove old mpools and Open/create the lock and mpool regions
	error_check_good env:close:$env [$env close] 0
	set ret [berkdb envremove -home $testdir]
	error_check_good env_remove $ret 0

	set env [berkdb env -create -cdb -home $testdir]
	error_check_good dbenv [is_valid_widget $env env] TRUE

	if { $do_exit == 1 } {
		return
	}

	# Now spawn off processes
	berkdb debug_check
	puts "\tTest042.b: forking off $procs children"
	set pidlist {}

	for { set i 0 } {$i < $procs} {incr i} {
		puts "exec $tclsh_path $test_path/wrap.tcl \
		    mdbscript.tcl $testdir/test042.$i.log \
		    $method $testdir $testfile $nentries $iter $i $procs &"
		set p [exec $tclsh_path $test_path/wrap.tcl \
		    mdbscript.tcl $testdir/test042.$i.log $method \
		    $testdir $testfile $nentries $iter $i $procs &]
		lappend pidlist $p
	}
	puts "Test042: $procs independent processes now running"
	watch_procs

	# Check for test failure
	set e [eval findfail [glob $testdir/test042.*.log]]
	error_check_good "FAIL: error message(s) in log files" $e 0

	# Test is done, blow away lock and mpool region
	reset_env $env
}

# If we are renumbering, then each time we delete an item, the number of
# items in the file is temporarily decreased, so the highest record numbers
# do not exist.  To make sure this doesn't happen, we never generate the
# highest few record numbers as keys.
#
# For record-based methods, record numbers begin at 1, while for other keys,
# we begin at 0 to index into an array.
proc rand_key { method nkeys renum procs} {
	if { $renum == 1 } {
		return [berkdb random_int 1 [expr $nkeys - $procs]]
	} elseif { [is_record_based $method] == 1 } {
		return [berkdb random_int 1 $nkeys]
	} else {
		return [berkdb random_int 0 [expr $nkeys - 1]]
	}
}
