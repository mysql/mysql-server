# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test064.tcl,v 11.8 2000/08/25 14:21:58 sue Exp $
#
# DB Test 64: Test of DB->get_type
#	Create a database of type specified by method.
#	Make sure DB->get_type returns the right thing with both a
#		normal and DB_UNKNOWN open.
proc test064 { method args } {
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]
	set tnum 64

	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test0$tnum.db
		set env NULL
	} else {
		set testfile test0$tnum.db
		incr eindex
		set env [lindex $args $eindex]
	}
	cleanup $testdir $env

	puts "Test0$tnum: $method ($args) DB->get_type test."

	# Create a test database.
	puts "\tTest0$tnum.a: Creating test database of type $method."
	set db [eval {berkdb_open -create -truncate -mode 0644} \
	    $omethod $args $testfile]
	error_check_good db_create [is_valid_db $db] TRUE

	error_check_good db_close [$db close] 0

	puts "\tTest0$tnum.b: get_type after method specifier."

	set db [eval {berkdb_open} $omethod $args {$testfile}]
	error_check_good db_open [is_valid_db $db] TRUE

	set type [$db get_type]
	error_check_good get_type $type [string range $omethod 1 end]

	error_check_good db_close [$db close] 0

	puts "\tTest0$tnum.c: get_type after DB_UNKNOWN."

	set db [eval {berkdb_open} $args $testfile]
	error_check_good db_open [is_valid_db $db] TRUE

	set type [$db get_type]
	error_check_good get_type $type [string range $omethod 1 end]

	error_check_good db_close [$db close] 0
}
