# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test088.tcl,v 11.4 2000/12/11 17:24:55 sue Exp $
#
# Test088: Cursor stability across btree splits with very deep trees. 
# (Variant of test048, SR #2514.)
proc test088 { method args } {
	global errorCode alphabet
	source ./include.tcl

	set tstn 088

	if { [is_btree $method] != 1 } {
		puts "Test$tstn skipping for method $method."
		return
	}
	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Test088: skipping for specific pagesizes"
		return
	}

	set method "-btree"

	puts "\tTest$tstn: Test of cursor stability across btree splits."

	set key	"key$alphabet$alphabet$alphabet"
	set data "data$alphabet$alphabet$alphabet"
	set txn ""
	set flags ""

	puts "\tTest$tstn.a: Create $method database."
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test$tstn.db
		set env NULL
	} else {
		set testfile test$tstn.db
		incr eindex
		set env [lindex $args $eindex]
	}
	set t1 $testdir/t1
	cleanup $testdir $env

	set ps 512 
	set oflags "-create -pagesize $ps -truncate -mode 0644 $args $method"
	set db [eval {berkdb_open} $oflags $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	set nkeys 5
	# Fill page w/ key/data pairs.
	#
	puts "\tTest$tstn.b: Fill page with $nkeys small key/data pairs."
	for { set i 0 } { $i < $nkeys } { incr i } {
		set ret [$db put ${key}00000$i $data$i]
		error_check_good dbput $ret 0
	}

	# get db ordering, set cursors
	puts "\tTest$tstn.c: Set cursors on each of $nkeys pairs."
	for {set i 0; set ret [$db get ${key}00000$i]} {\
			$i < $nkeys && [llength $ret] != 0} {\
			incr i; set ret [$db get ${key}00000$i]} {
		set key_set($i) [lindex [lindex $ret 0] 0]
		set data_set($i) [lindex [lindex $ret 0] 1]
		set dbc [$db cursor]
		set dbc_set($i) $dbc
		error_check_good db_cursor:$i [is_substr $dbc_set($i) $db] 1
		set ret [$dbc_set($i) get -set $key_set($i)]
		error_check_bad dbc_set($i)_get:set [llength $ret] 0
	}

	# if mkeys is above 1000, need to adjust below for lexical order
	set mkeys 30000
	puts "\tTest$tstn.d: Add $mkeys pairs to force splits."
	for {set i $nkeys} { $i < $mkeys } { incr i } {
		if { $i >= 10000 } {
			set ret [$db put ${key}0$i $data$i]
		} elseif { $i >= 1000 } {
			set ret [$db put ${key}00$i $data$i]
		} elseif { $i >= 100 } {
			set ret [$db put ${key}000$i $data$i]
		} elseif { $i >= 10 } {
			set ret [$db put ${key}0000$i $data$i]
		} else {
			set ret [$db put ${key}00000$i $data$i]
		}
		error_check_good dbput:more $ret 0
	}

	puts "\tTest$tstn.e: Make sure splits happened."
	error_check_bad stat:check-split [is_substr [$db stat] \
					"{{Internal pages} 0}"] 1

	puts "\tTest$tstn.f: Check to see that cursors maintained reference."
	for {set i 0} { $i < $nkeys } {incr i} {
		set ret [$dbc_set($i) get -current]
		error_check_bad dbc$i:get:current [llength $ret] 0
		set ret2 [$dbc_set($i) get -set $key_set($i)]
		error_check_bad dbc$i:get:set [llength $ret2] 0
		error_check_good dbc$i:get(match) $ret $ret2
	}

	puts "\tTest$tstn.g: Delete added keys to force reverse splits."
	for {set i $nkeys} { $i < $mkeys } { incr i } {
		if { $i >= 10000 } {
			error_check_good db_del:$i [$db del ${key}0$i] 0
		} elseif { $i >= 1000 } {
			error_check_good db_del:$i [$db del ${key}00$i] 0
		} elseif { $i >= 100 } {
			error_check_good db_del:$i [$db del ${key}000$i] 0
		} elseif { $i >= 10 } {
			error_check_good db_del:$i [$db del ${key}0000$i] 0
		} else {
			error_check_good db_del:$i [$db del ${key}00000$i] 0
		}
	}

	puts "\tTest$tstn.h: Verify cursor reference."
	for {set i 0} { $i < $nkeys } {incr i} {
		set ret [$dbc_set($i) get -current]
		error_check_bad dbc$i:get:current [llength $ret] 0
		set ret2 [$dbc_set($i) get -set $key_set($i)]
		error_check_bad dbc$i:get:set [llength $ret2] 0
		error_check_good dbc$i:get(match) $ret $ret2
	}

	puts "\tTest$tstn.i: Cleanup."
	# close cursors
	for {set i 0} { $i < $nkeys } {incr i} {
		error_check_good dbc_close:$i [$dbc_set($i) close] 0
	}
	error_check_good dbclose [$db close] 0

	puts "\tTest$tstn complete."
}
