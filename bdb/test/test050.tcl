# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test050.tcl,v 11.15 2000/08/25 14:21:57 sue Exp $
#
# Test050: Overwrite test of small/big key/data with cursor checks for RECNO
proc test050 { method args } {
	global alphabet
	global errorInfo
	global errorCode
	source ./include.tcl

	set tstn 050

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_rrecno $method] != 1 } {
		puts "Test$tstn skipping for method $method."
		return
	}

	puts "\tTest$tstn:\
	    Overwrite test with cursor and small/big key/data ($method)."

	set data "data"
	set txn ""
	set flags ""

	puts "\tTest$tstn: Create $method database."
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test0$tstn.db
		set env NULL
	} else {
		set testfile test0$tstn.db
		incr eindex
		set env [lindex $args $eindex]
	}
	set t1 $testdir/t1
	cleanup $testdir $env

	set oflags "-create -truncate -mode 0644 $args $omethod"
	set db [eval {berkdb_open_noerr} $oflags $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	# open curs to db
	set dbc [$db cursor]
	error_check_good db_cursor [is_substr $dbc $db] 1

	# keep nkeys even
	set nkeys 20

	# Fill page w/ small key/data pairs
	#
	puts "\tTest$tstn: Fill page with $nkeys small key/data pairs."
	for { set i 1 } { $i <= $nkeys } { incr i } {
			set ret [$db put $i [chop_data $method $data$i]]
			error_check_good dbput $ret 0
	}

	# get db order of keys
	for {set i 0; set ret [$dbc get -first]} { [llength $ret] != 0} { \
			set ret [$dbc get -next]} {
		set key_set($i) [lindex [lindex $ret 0] 0]
		set data_set($i) [lindex [lindex $ret 0] 1]
		incr i
	}

	# verify ordering: should be unnecessary, but hey, why take chances?
	#	key_set is zero indexed but keys start at 1
	for {set i 0} { $i < $nkeys } {incr i} {
		error_check_good \
		    verify_order:$i $key_set($i) [pad_data $method [expr $i+1]]
	}

	puts "\tTest$tstn.a: Inserts before/after by cursor."
	puts "\t\tTest$tstn.a.1:\
	    Insert with uninitialized cursor (should fail)."
	error_check_good dbc_close [$dbc close] 0
	set dbc [$db cursor]
	error_check_good db_cursor [is_substr $dbc $db] 1
	catch {$dbc put -before DATA1} ret
	error_check_good dbc_put:before:uninit [is_substr $errorCode EINVAL] 1

	catch {$dbc put -after DATA2} ret
	error_check_good dbc_put:after:uninit [is_substr $errorCode EINVAL] 1

	puts "\t\tTest$tstn.a.2: Insert with deleted cursor (should succeed)."
	set ret [$dbc get -first]
	error_check_bad dbc_get:first [llength $ret] 0
	error_check_good dbc_del [$dbc del] 0
	set ret [$dbc put -current DATAOVER1]
	error_check_good dbc_put:current:deleted $ret 0

	puts "\t\tTest$tstn.a.3: Insert by cursor before cursor (DB_BEFORE)."
	set currecno [lindex [lindex [$dbc get -current] 0] 0]
	set ret [$dbc put -before DATAPUTBEFORE]
	error_check_good dbc_put:before $ret $currecno
	set old1 [$dbc get -next]
	error_check_bad dbc_get:next [llength $old1] 0
	error_check_good \
	    dbc_get:next(compare) [lindex [lindex $old1 0] 1] DATAOVER1

	puts "\t\tTest$tstn.a.4: Insert by cursor after cursor (DB_AFTER)."
	set ret [$dbc get -first]
	error_check_bad dbc_get:first [llength $ret] 0
	error_check_good dbc_get:first [lindex [lindex $ret 0] 1] DATAPUTBEFORE
	set currecno [lindex [lindex [$dbc get -current] 0] 0]
	set ret [$dbc put -after DATAPUTAFTER]
	error_check_good dbc_put:after $ret [expr $currecno + 1]
	set ret [$dbc get -prev]
	error_check_bad dbc_get:prev [llength $ret] 0
	error_check_good \
	    dbc_get:prev [lindex [lindex $ret 0] 1] DATAPUTBEFORE

	puts "\t\tTest$tstn.a.5: Verify that all keys have been renumbered."
	# should be $nkeys + 2 keys, starting at 1
	for {set i 1; set ret [$dbc get -first]} { \
			$i <= $nkeys && [llength $ret] != 0 } {\
			incr i; set ret [$dbc get -next]} {
		error_check_good check_renumber $i [lindex [lindex $ret 0] 0]
	}

	# tested above

	puts "\tTest$tstn.b: Overwrite tests (cursor and key)."
	# For the next part of the test, we need a db with no dups to test
	# overwrites
	#
	# we should have ($nkeys + 2) keys, ordered:
	#	DATAPUTBEFORE, DATAPUTAFTER, DATAOVER1, data1, ..., data$nkeys
	#
	# Prepare cursor on item
	#
	set ret [$dbc get -first]
	error_check_bad dbc_get:first [llength $ret] 0

	# Prepare unique big/small values for an initial
	# and an overwrite set of data
	set databig DATA_BIG_[repeat alphabet 250]
	set datasmall DATA_SMALL

	# Now, we want to overwrite data:
	#  by key and by cursor
	#  1. small by small
	#	2. small by big
	#	3. big by small
	#	4. big by big
	#
	set i 0
	# Do all overwrites for key and cursor
	foreach type { by_key by_cursor } {
		incr i
		puts "\tTest$tstn.b.$i: Overwrites $type."
		foreach pair { {small small} \
		    {small big} {big small} {big big} } {
			# put in initial type
			set data $data[lindex $pair 0]
			set ret [$dbc put -current $data]
			error_check_good dbc_put:curr:init:($pair) $ret 0

			# Now, try to overwrite: dups not supported in this db
			if { [string compare $type by_key] == 0 } {
				puts "\t\tTest$tstn.b.$i:\
				    Overwrite:($pair):$type"
				set ret [$db put \
				    1 OVER$pair$data[lindex $pair 1]]
				error_check_good dbput:over:($pair) $ret 0
			} else {
				# This is a cursor overwrite
				puts "\t\tTest$tstn.b.$i:\
				    Overwrite:($pair) by cursor."
				set ret [$dbc put \
				    -current OVER$pair$data[lindex $pair 1]]
				error_check_good dbcput:over:($pair) $ret 0
			}
		} ;# foreach pair
	} ;# foreach type key/cursor

	puts "\tTest$tstn.c: Cleanup and close cursor."
	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0

	puts "\tTest$tstn complete."
}
