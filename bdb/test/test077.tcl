# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test077.tcl,v 1.4 2000/08/25 14:21:58 sue Exp $
#
# DB Test 77: Test of DB_GET_RECNO [#1206].
proc test077 { method { nkeys 1000 } { pagesize 512 } { tnum 77 } args } {
	source ./include.tcl
	global alphabet

	set omethod [convert_method $method]
	set args [convert_args $method $args]

	puts "Test0$tnum: Test of DB_GET_RECNO."

	if { [is_rbtree $method] != 1 } {
		puts "\tTest0$tnum: Skipping for method $method."
		return
	}

	set data $alphabet

	set eindex [lsearch -exact $args "-env"]
	if { $eindex == -1 } {
		set testfile $testdir/test0$tnum.db
		set env NULL
	} else {
		set testfile test0$tnum.db
		incr eindex
		set env [lindex $args $eindex]
	}
	cleanup $testdir $env

	set db [eval {berkdb_open -create -truncate -mode 0644\
	    -pagesize $pagesize} $omethod $args {$testfile}]
	error_check_good db_open [is_valid_db $db] TRUE

	puts "\tTest0$tnum.a: Populating database."

	for { set i 1 } { $i <= $nkeys } { incr i } {
		set key [format %5d $i]
		error_check_good db_put($key) [$db put $key $data] 0
	}

	puts "\tTest0$tnum.b: Verifying record numbers."

	set dbc [$db cursor]
	error_check_good dbc_open [is_valid_cursor $dbc $db] TRUE

	set i 1
	for { set dbt [$dbc get -first] } \
	    { [string length $dbt] != 0 } \
	    { set dbt [$dbc get -next] } {
		set recno [$dbc get -get_recno]
		set keynum [expr [lindex [lindex $dbt 0] 0]]

		# Verify that i, the number that is the key, and recno
		# are all equal.
		error_check_good key($i) $keynum $i
		error_check_good recno($i) $recno $i
		incr i
	}

	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0
}
