# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test066.tcl,v 11.7 2000/08/25 14:21:58 sue Exp $
#
# DB Test 66: Make sure a cursor put to DB_CURRENT acts as an overwrite in
# a database with duplicates
proc test066 { method args } {
	set omethod [convert_method $method]
	set args [convert_args $method $args]

	set tnum 66

	if { [is_record_based $method] || [is_rbtree $method] } {
	    puts "Test0$tnum: Skipping for method $method."
	    return
	}

	puts "Test0$tnum: Test of cursor put to DB_CURRENT with duplicates."

	source ./include.tcl

	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test066.db
		set env NULL
	} else {
		set testfile test066.db
		incr eindex
		set env [lindex $args $eindex]
	}
	cleanup $testdir $env

	set key "test"
	set data "olddata"

	set db [eval {berkdb_open -create -mode 0644 -dup} $omethod $args \
	    $testfile]
	error_check_good db_open [is_valid_db $db] TRUE

	set ret [eval {$db put} $key [chop_data $method $data]]
	error_check_good db_put $ret 0

	set dbc [$db cursor]
	error_check_good db_cursor [is_valid_cursor $dbc $db] TRUE

	set ret [$dbc get -first]
	error_check_good db_get $ret [list [list $key [pad_data $method $data]]]

	set newdata "newdata"
	set ret [$dbc put -current [chop_data $method $newdata]]
	error_check_good dbc_put $ret 0

	# There should be only one (key,data) pair in the database, and this
	# is it.
	set ret [$dbc get -first]
	error_check_good db_get_first $ret \
	    [list [list $key [pad_data $method $newdata]]]

	# and this one should come up empty.
	set ret [$dbc get -next]
	error_check_good db_get_next $ret ""

	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0

	puts "\tTest0$tnum: Test completed successfully."
}
