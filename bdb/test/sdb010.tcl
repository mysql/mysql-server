# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: sdb010.tcl,v 11.4 2000/08/25 14:21:53 sue Exp $
#
# Subdatabase Test 10 {access method}
# Test of dbremove
proc subdb010 { method args } {
	global errorCode
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	puts "Subdb010: Test of DB->remove()"

	if { [is_queue $method] == 1 } {
		puts "\tSubdb010: Skipping for method $method."
		return
	}

	cleanup $testdir NULL

	set testfile $testdir/subdb010.db
	set testdb DATABASE

	set db [eval {berkdb_open -create -truncate -mode 0644} $omethod \
	    $args $testfile $testdb]
	error_check_good db_open [is_valid_db $db] TRUE
	error_check_good db_close [$db close] 0

	error_check_good file_exists_before [file exists $testfile] 1
	error_check_good db_remove [berkdb dbremove $testfile $testdb] 0

	# File should still exist.
	error_check_good file_exists_after [file exists $testfile] 1

	# But database should not.
	set ret [catch {eval berkdb_open $omethod $args $testfile $testdb} res]
	error_check_bad open_failed ret 0
	error_check_good open_failed_ret [is_substr $errorCode ENOENT] 1

	puts "\tSubdb010 succeeded."
}
