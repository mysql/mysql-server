# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test063.tcl,v 11.11 2000/08/25 14:21:58 sue Exp $
#
# DB Test 63:  Test that the DB_RDONLY flag is respected.
#	Attempt to both DB->put and DBC->c_put into a database
#	that has been opened DB_RDONLY, and check for failure.
proc test063 { method args } {
	global errorCode
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]
	set tnum 63

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

	set key "key"
	set data "data"
	set key2 "another_key"
	set data2 "more_data"

	set gflags ""

	if { [is_record_based $method] == 1 } {
	    set key "1"
	    set key2 "2"
	    append gflags " -recno"
	}

	puts "Test0$tnum: $method ($args) DB_RDONLY test."

	# Create a test database.
	puts "\tTest0$tnum.a: Creating test database."
	set db [eval {berkdb_open_noerr -create -truncate -mode 0644} \
	    $omethod $args $testfile]
	error_check_good db_create [is_valid_db $db] TRUE

	# Put and get an item so it's nonempty.
	set ret [eval {$db put} $key [chop_data $method $data]]
	error_check_good initial_put $ret 0

	set dbt [eval {$db get} $gflags $key]
	error_check_good initial_get $dbt \
	    [list [list $key [pad_data $method $data]]]

	error_check_good db_close [$db close] 0

	if { $eindex == -1 } {
		# Confirm that database is writable.  If we are
		# using an env (that may be remote on a server)
		# we cannot do this check.
		error_check_good writable [file writable $testfile] 1
	}

	puts "\tTest0$tnum.b: Re-opening DB_RDONLY and attempting to put."

	# Now open it read-only and make sure we can get but not put.
	set db [eval {berkdb_open_noerr -rdonly} $args {$testfile}]
	error_check_good db_open [is_valid_db $db] TRUE

	set dbt [eval {$db get} $gflags $key]
	error_check_good db_get $dbt \
	    [list [list $key [pad_data $method $data]]]

	set ret [catch {eval {$db put} $key2 [chop_data $method $data]} res]
	error_check_good put_failed $ret 1
	error_check_good db_put_rdonly [is_substr $errorCode "EACCES"] 1

	set errorCode "NONE"

	puts "\tTest0$tnum.c: Attempting cursor put."

	set dbc [$db cursor]
	error_check_good cursor_create [is_valid_cursor $dbc $db] TRUE

	error_check_good cursor_set [$dbc get -first] $dbt
	set ret [catch {eval {$dbc put} -current $data} res]
	error_check_good c_put_failed $ret 1
	error_check_good dbc_put_rdonly [is_substr $errorCode "EACCES"] 1

	set dbt [eval {$db get} $gflags $key2]
	error_check_good db_get_key2 $dbt ""

	puts "\tTest0$tnum.d: Attempting ordinary delete."

	set errorCode "NONE"
	set ret [catch {eval {$db del} $key} 1]
	error_check_good del_failed $ret 1
	error_check_good db_del_rdonly [is_substr $errorCode "EACCES"] 1

	set dbt [eval {$db get} $gflags $key]
	error_check_good db_get_key $dbt \
	    [list [list $key [pad_data $method $data]]]

	puts "\tTest0$tnum.e: Attempting cursor delete."
	# Just set the cursor to the beginning;  we don't care what's there...
	# yet.
	set dbt2 [$dbc get -first]
	error_check_good db_get_first_key $dbt2 $dbt
	set errorCode "NONE"
	set ret [catch {$dbc del} res]
	error_check_good c_del_failed $ret 1
	error_check_good dbc_del_rdonly [is_substr $errorCode "EACCES"] 1

	set dbt2 [$dbc get -current]
	error_check_good db_get_key $dbt2 $dbt

	puts "\tTest0$tnum.f: Close, reopen db;  verify unchanged."

	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0

	set db [eval {berkdb_open} $omethod $args $testfile]
	error_check_good db_reopen [is_valid_db $db] TRUE

	set dbc [$db cursor]
	error_check_good cursor_create [is_valid_cursor $dbc $db] TRUE

	error_check_good first_there [$dbc get -first] \
	    [list [list $key [pad_data $method $data]]]
	error_check_good nomore_there [$dbc get -next] ""

	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0
}
