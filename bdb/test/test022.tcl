# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test022.tcl,v 11.10 2000/08/25 14:21:55 sue Exp $
#
# Test022: Test of DB->get_byteswapped
proc test022 { method args } {
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	puts "Test022 ($args) $omethod: DB->getbyteswapped()"

	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile1 "$testdir/test022a.db"
		set testfile2 "$testdir/test022b.db"
		set env NULL
	} else {
		set testfile1 "test022a.db"
		set testfile2 "test022b.db"
		incr eindex
		set env [lindex $args $eindex]
	}
	cleanup $testdir $env

	# Create two databases, one in each byte order.
	set db1 [eval {berkdb_open -create \
	    -mode 0644} $omethod $args {-lorder 1234} $testfile1]
	error_check_good db1_open [is_valid_db $db1] TRUE

	set db2 [eval {berkdb_open -create \
	    -mode 0644} $omethod $args {-lorder 4321} $testfile2]
	error_check_good db2_open [is_valid_db $db2] TRUE

	# Call DB->get_byteswapped on both of them.
	set db1_order [$db1 is_byteswapped]
	set db2_order [$db2 is_byteswapped]

	# Make sure that both answers are either 1 or 0,
	# and that exactly one of them is 1.
	error_check_good is_byteswapped_sensible_1 \
	    [expr ($db1_order == 1 && $db2_order == 0) || \
		  ($db1_order == 0 && $db2_order == 1)] 1

	error_check_good db1_close [$db1 close] 0
	error_check_good db2_close [$db2 close] 0
	puts "\tTest022 complete."
}
