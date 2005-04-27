# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: test070.tcl,v 11.27 2002/09/05 17:23:07 sandstro Exp $
#
# TEST	test070
# TEST	Test of DB_CONSUME (Four consumers, 1000 items.)
# TEST
# TEST	Fork off six processes, four consumers and two producers.
# TEST	The producers will each put 20000 records into a queue;
# TEST	the consumers will each get 10000.
# TEST	Then, verify that no record was lost or retrieved twice.
proc test070 { method {nconsumers 4} {nproducers 2} \
    {nitems 1000} {mode CONSUME } {start 0} {txn -txn} {tnum 70} args } {
	source ./include.tcl
	global alphabet
	global encrypt

	#
	# If we are using an env, then skip this test.  It needs its own.
	set eindex [lsearch -exact $args "-env"]
	if { $eindex != -1 } {
		incr eindex
		set env [lindex $args $eindex]
		puts "Test0$tnum skipping for env $env"
		return
	}
	set omethod [convert_method $method]
	set args [convert_args $method $args]
	if { $encrypt != 0 } {
		puts "Test0$tnum skipping for security"
		return
	}

	puts "Test0$tnum: $method ($args) Test of DB_$mode flag to DB->get."
	puts "\tUsing $txn environment."

	error_check_good enough_consumers [expr $nconsumers > 0] 1
	error_check_good enough_producers [expr $nproducers > 0] 1

	if { [is_queue $method] != 1 } {
		puts "\tSkipping Test0$tnum for method $method."
		return
	}

	env_cleanup $testdir
	set testfile test0$tnum.db

	# Create environment
	set dbenv [eval {berkdb_env -create $txn -home } $testdir]
	error_check_good dbenv_create [is_valid_env $dbenv] TRUE

	# Create database
	set db [eval {berkdb_open -create -mode 0644 -queue}\
		-env $dbenv $args $testfile]
	error_check_good db_open [is_valid_db $db] TRUE

	if { $start != 0 } {
		error_check_good set_seed [$db put $start "consumer data"] 0
		puts "\tStarting at $start."
	} else {
		incr start
	}

	set pidlist {}

	# Divvy up the total number of records amongst the consumers and
	# producers.
	error_check_good cons_div_evenly [expr $nitems % $nconsumers] 0
	error_check_good prod_div_evenly [expr $nitems % $nproducers] 0
	set nperconsumer [expr $nitems / $nconsumers]
	set nperproducer [expr $nitems / $nproducers]

	set consumerlog $testdir/CONSUMERLOG.

	# Fork consumer processes (we want them to be hungry)
	for { set ndx 0 } { $ndx < $nconsumers } { incr ndx } {
		set output $consumerlog$ndx
		set p [exec $tclsh_path $test_path/wrap.tcl \
		    conscript.tcl $testdir/conscript.log.consumer$ndx \
		    $testdir $testfile $mode $nperconsumer $output $tnum \
		    $args &]
		lappend pidlist $p
	}
	for { set ndx 0 } { $ndx < $nproducers } { incr ndx } {
		set p [exec $tclsh_path $test_path/wrap.tcl \
		    conscript.tcl $testdir/conscript.log.producer$ndx \
		    $testdir $testfile PRODUCE $nperproducer "" $tnum \
		    $args &]
		lappend pidlist $p
	}

	# Wait for all children.
	watch_procs $pidlist 10

	# Verify: slurp all record numbers into list, sort, and make
	# sure each appears exactly once.
	puts "\tTest0$tnum: Verifying results."
	set reclist {}
	for { set ndx 0 } { $ndx < $nconsumers } { incr ndx } {
		set input $consumerlog$ndx
		set iid [open $input r]
		while { [gets $iid str] != -1 } {
			# Convert high ints to negative ints, to
			# simulate Tcl's behavior on a 32-bit machine
			# even if we're on a 64-bit one.
			if { $str > 0x7fffffff } {
				set str [expr $str - 1 - 0xffffffff]
			}
			lappend reclist $str
		}
		close $iid
	}
	set sortreclist [lsort -integer $reclist]

	set nitems [expr $start + $nitems]
	for { set ndx $start } { $ndx < $nitems } { incr ndx } {
		# Convert high ints to negative ints, to simulate
		# 32-bit behavior on 64-bit platforms.
		if { $ndx > 0x7fffffff } {
			set cmp [expr $ndx - 1 - 0xffffffff]
		} else {
			set cmp [expr $ndx + 0]
		}
		# Skip 0 if we are wrapping around
		if { $cmp == 0 } {
			incr ndx
			incr nitems
			incr cmp
		}
		# Be sure to convert ndx to a number before comparing.
		error_check_good pop_num [lindex $sortreclist 0] $cmp
		set sortreclist [lreplace $sortreclist 0 0]
	}
	error_check_good list_ends_empty $sortreclist {}
	error_check_good db_close [$db close] 0
	error_check_good dbenv_close [$dbenv close] 0

	puts "\tTest0$tnum completed successfully."
}
