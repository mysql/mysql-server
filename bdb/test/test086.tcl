# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test086.tcl,v 11.2 2000/08/25 14:21:58 sue Exp $

# Test086: Cursor stability across btree splits w/ subtransaction abort [#2373].
proc test086 { method args } {
	global errorCode
	source ./include.tcl

	set tstn 086

	if { [is_btree $method] != 1 } {
		puts "Test$tstn skipping for method $method."
		return
	}

	set method "-btree"

	puts "\tTest$tstn: Test of cursor stability across aborted\
	    btree splits."

	set key "key"
	set data "data"
	set txn ""
	set flags ""

	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then this test won't work.
	if { $eindex == -1 } {
		# But we will be using our own env...
		set testfile test0$tstn.db
	} else {
		puts "\tTest$tstn: Environment provided;  skipping test."
		return
	}
	set t1 $testdir/t1
	env_cleanup $testdir

	set env [berkdb env -create -home $testdir -txn]
	error_check_good berkdb_env [is_valid_env $env] TRUE

	puts "\tTest$tstn.a: Create $method database."
	set oflags "-create -env $env -mode 0644 $args $method"
	set db [eval {berkdb_open} $oflags $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	set nkeys 5
	# Fill page w/ small key/data pairs, keep at leaf
	#
	puts "\tTest$tstn.b: Fill page with $nkeys small key/data pairs."
	set txn [$env txn]
	error_check_good txn [is_valid_txn $txn $env] TRUE
	for { set i 0 } { $i < $nkeys } { incr i } {
		set ret [$db put -txn $txn key000$i $data$i]
		error_check_good dbput $ret 0
	}
	error_check_good commit [$txn commit] 0

	# get db ordering, set cursors
	puts "\tTest$tstn.c: Set cursors on each of $nkeys pairs."
	set txn [$env txn]
	error_check_good txn [is_valid_txn $txn $env] TRUE
	for {set i 0; set ret [$db get -txn $txn key000$i]} {\
			$i < $nkeys && [llength $ret] != 0} {\
			incr i; set ret [$db get -txn $txn key000$i]} {
		set key_set($i) [lindex [lindex $ret 0] 0]
		set data_set($i) [lindex [lindex $ret 0] 1]
		set dbc [$db cursor -txn $txn]
		set dbc_set($i) $dbc
		error_check_good db_cursor:$i [is_substr $dbc_set($i) $db] 1
		set ret [$dbc_set($i) get -set $key_set($i)]
		error_check_bad dbc_set($i)_get:set [llength $ret] 0
	}

	# Create child txn.
	set ctxn [$env txn -parent $txn]
	error_check_good ctxn [is_valid_txn $txn $env] TRUE

	# if mkeys is above 1000, need to adjust below for lexical order
	set mkeys 1000
	puts "\tTest$tstn.d: Add $mkeys pairs to force split."
	for {set i $nkeys} { $i < $mkeys } { incr i } {
		if { $i >= 100 } {
			set ret [$db put -txn $ctxn key0$i $data$i]
		} elseif { $i >= 10 } {
			set ret [$db put -txn $ctxn key00$i $data$i]
		} else {
			set ret [$db put -txn $ctxn key000$i $data$i]
		}
		error_check_good dbput:more $ret 0
	}

	puts "\tTest$tstn.e: Abort."
	error_check_good ctxn_abort [$ctxn abort] 0


	puts "\tTest$tstn.f: Check and see that cursors maintained reference."
	for {set i 0} { $i < $nkeys } {incr i} {
		set ret [$dbc_set($i) get -current]
		error_check_bad dbc$i:get:current [llength $ret] 0
		set ret2 [$dbc_set($i) get -set $key_set($i)]
		error_check_bad dbc$i:get:set [llength $ret2] 0
		error_check_good dbc$i:get(match) $ret $ret2
	}

	# Put (and this time keep) the keys that caused the split.  
	# We'll delete them to test reverse splits.
	puts "\tTest$tstn.g: Put back added keys."
	for {set i $nkeys} { $i < $mkeys } { incr i } {
		if { $i >= 100 } {
			set ret [$db put -txn $txn key0$i $data$i]
		} elseif { $i >= 10 } {
			set ret [$db put -txn $txn key00$i $data$i]
		} else {
			set ret [$db put -txn $txn key000$i $data$i]
		}
		error_check_good dbput:more $ret 0
	}

	puts "\tTest$tstn.h: Delete added keys to force reverse split."
	set ctxn [$env txn -parent $txn]
	error_check_good ctxn [is_valid_txn $txn $env] TRUE
	for {set i $nkeys} { $i < $mkeys } { incr i } {
		if { $i >= 100 } {
			error_check_good db_del:$i [$db del -txn $ctxn key0$i] 0
		} elseif { $i >= 10 } {
			error_check_good db_del:$i \
			    [$db del -txn $ctxn key00$i] 0
		} else {
			error_check_good db_del:$i \
			    [$db del -txn $ctxn key000$i] 0
		}
	}

	puts "\tTest$tstn.i: Abort."
	error_check_good ctxn_abort [$ctxn abort] 0

	puts "\tTest$tstn.j: Verify cursor reference."
	for {set i 0} { $i < $nkeys } {incr i} {
		set ret [$dbc_set($i) get -current]
		error_check_bad dbc$i:get:current [llength $ret] 0
		set ret2 [$dbc_set($i) get -set $key_set($i)]
		error_check_bad dbc$i:get:set [llength $ret2] 0
		error_check_good dbc$i:get(match) $ret $ret2
	}

	puts "\tTest$tstn.j: Cleanup."
	# close cursors
	for {set i 0} { $i < $nkeys } {incr i} {
		error_check_good dbc_close:$i [$dbc_set($i) close] 0
	}

	error_check_good commit [$txn commit] 0
	error_check_good dbclose [$db close] 0
	error_check_good envclose [$env close] 0

	puts "\tTest$tstn complete."
}
