# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test047.tcl,v 11.10 2000/08/25 14:21:56 sue Exp $
#
# DB Test 47: test of the SET_RANGE interface to DB->c_get.
proc test047 { method args } {
	source ./include.tcl

	set tstn 047

	if { [is_btree $method] != 1 } {
		puts "Test$tstn skipping for method $method"
		return
	}

	set method "-btree"

	puts "\tTest$tstn: Test of SET_RANGE interface to DB->c_get ($method)."

	set key	"key"
	set data	"data"
	set txn ""
	set flags ""

	puts "\tTest$tstn.a: Create $method database."
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test0$tstn.db
		set testfile1 $testdir/test0$tstn.a.db
		set testfile2 $testdir/test0$tstn.b.db
		set env NULL
	} else {
		set testfile test0$tstn.db
		set testfile1 test0$tstn.a.db
		set testfile2 test0$tstn.b.db
		incr eindex
		set env [lindex $args $eindex]
	}
	set t1 $testdir/t1
	cleanup $testdir $env

	set oflags "-create -truncate -mode 0644 -dup $args $method"
	set db [eval {berkdb_open} $oflags $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	# open curs to db
	set dbc [$db cursor]
	error_check_good db_cursor [is_substr $dbc $db] 1

	set nkeys 20
	# Fill page w/ small key/data pairs
	#
	puts "\tTest$tstn.b: Fill page with $nkeys small key/data pairs."
	for { set i 0 } { $i < $nkeys } { incr i } {
		set ret [$db put $key$i $data$i]
		error_check_good dbput $ret 0
	}

	puts "\tTest$tstn.c: Get data with SET_RANGE, then delete by cursor."
	set i 0
	set ret [$dbc get -set_range $key$i]
	error_check_bad dbc_get:set_range [llength $ret] 0
	set curr $ret

	# delete by cursor, make sure it is gone
	error_check_good dbc_del [$dbc del] 0

	set ret [$dbc get -set_range $key$i]
	error_check_bad dbc_get(post-delete):set_range [llength $ret] 0
	error_check_bad dbc_get(no-match):set_range $ret $curr

	puts "\tTest$tstn.d: \
	    Use another cursor to fix item on page, delete by db."
	set dbcurs2 [$db cursor]
	error_check_good db:cursor2 [is_substr $dbcurs2 $db] 1

	set ret [$dbcurs2 get -set [lindex [lindex $ret 0] 0]]
	error_check_bad dbc_get(2):set [llength $ret] 0
	set curr $ret
	error_check_good db:del [$db del [lindex [lindex $ret 0] 0]] 0

	# make sure item is gone
	set ret [$dbcurs2 get -set_range [lindex [lindex $curr 0] 0]]
	error_check_bad dbc2_get:set_range [llength $ret] 0
	error_check_bad dbc2_get:set_range $ret $curr

	puts "\tTest$tstn.e: Close for second part of test, close db/cursors."
	error_check_good dbc:close [$dbc close] 0
	error_check_good dbc2:close [$dbcurs2 close] 0
	error_check_good dbclose [$db close] 0

	# open db
	set db [eval {berkdb_open} $oflags $testfile1]
	error_check_good dbopen2 [is_valid_db $db] TRUE

	set nkeys 10
	puts "\tTest$tstn.f: Fill page with $nkeys pairs, one set of dups."
	for {set i 0} { $i < $nkeys } {incr i} {
		# a pair
		set ret [$db put $key$i $data$i]
		error_check_good dbput($i) $ret 0
	}

	set j 0
	for {set i 0} { $i < $nkeys } {incr i} {
		# a dup set for same  1 key
		set ret [$db put $key$i DUP_$data$i]
		error_check_good dbput($i):dup $ret 0
	}

	puts "\tTest$tstn.g: \
	    Get dups key w/ SET_RANGE, pin onpage with another cursor."
	set i 0
	set dbc [$db cursor]
	error_check_good db_cursor [is_substr $dbc $db] 1
	set ret [$dbc get -set_range $key$i]
	error_check_bad dbc_get:set_range [llength $ret] 0

	set dbc2 [$db cursor]
	error_check_good db_cursor2 [is_substr $dbc2 $db] 1
	set ret2 [$dbc2 get -set_range $key$i]
	error_check_bad dbc2_get:set_range [llength $ret] 0

	error_check_good dbc_compare $ret $ret2
	puts "\tTest$tstn.h: \
	    Delete duplicates' key, use SET_RANGE to get next dup."
	set ret [$dbc2 del]
	error_check_good dbc2_del $ret 0
	set ret [$dbc get -set_range $key$i]
	error_check_bad dbc_get:set_range [llength $ret] 0
	error_check_bad dbc_get:set_range $ret $ret2

	error_check_good dbc_close [$dbc close] 0
	error_check_good dbc2_close [$dbc2 close] 0
	error_check_good db_close [$db close] 0

	set db [eval {berkdb_open} $oflags $testfile2]
	error_check_good dbopen [is_valid_db $db] TRUE
	set dbc [$db cursor]
	error_check_good db_cursor [is_substr $dbc $db] 1
	set dbc2 [$db cursor]
	error_check_good db_cursor2 [is_substr $dbc2 $db] 1

	set nkeys 10
	set ndups 1000

	puts "\tTest$tstn.i: Fill page with $nkeys pairs and $ndups dups."
	for {set i 0} { $i < $nkeys } { incr i} {
		# a pair
		set ret [$db put $key$i $data$i]
		error_check_good dbput $ret 0

		# dups for single pair
		if { $i == 0} {
			for {set j 0} { $j < $ndups } { incr j } {
				set ret [$db put $key$i DUP_$data$i:$j]
				error_check_good dbput:dup $ret 0
			}
		}
	}
	set i 0
	puts "\tTest$tstn.j: \
	    Get key of first dup with SET_RANGE, fix with 2 curs."
	set ret [$dbc get -set_range $key$i]
	error_check_bad dbc_get:set_range [llength $ret] 0

	set ret2 [$dbc2 get -set_range $key$i]
	error_check_bad dbc2_get:set_range [llength $ret] 0
	set curr $ret2

	error_check_good dbc_compare $ret $ret2

	puts "\tTest$tstn.k: Delete item by cursor, use SET_RANGE to verify."
	set ret [$dbc2 del]
	error_check_good dbc2_del $ret 0
	set ret [$dbc get -set_range $key$i]
	error_check_bad dbc_get:set_range [llength $ret] 0
	error_check_bad dbc_get:set_range $ret $curr

	puts "\tTest$tstn.l: Cleanup."
	error_check_good dbc_close [$dbc close] 0
	error_check_good dbc2_close [$dbc2 close] 0
	error_check_good db_close [$db close] 0

	puts "\tTest$tstn complete."
}
