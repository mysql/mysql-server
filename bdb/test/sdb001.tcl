# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: sdb001.tcl,v 11.12 2000/08/25 14:21:52 sue Exp $
#
# Sub DB Test 1 {access method}
# Test non-subdb and subdb operations
# Test naming (filenames begin with -)
# Test existence (cannot create subdb of same name with -excl)
proc subdb001 { method args } {
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	puts "Subdb001: $method ($args) subdb and non-subdb tests"

	# Create the database and open the dictionary
	set testfile $testdir/subdb001.db
	set subdb subdb0
	cleanup $testdir NULL
	puts "\tSubdb001.a: Non-subdb database and subdb operations"
	#
	# Create a db with no subdbs.  Add some data.  Close.  Try to
	# open/add with a subdb.  Should fail.
	#
	puts "\tSubdb001.a.0: Create db, add data, close, try subdb"
	set db [eval {berkdb_open -create -truncate -mode 0644} \
	    $args {$omethod $testfile}]
	error_check_good dbopen [is_valid_db $db] TRUE

	set did [open $dict]

	set pflags ""
	set gflags ""
	set txn ""
	set count 0

	if { [is_record_based $method] == 1 } {
		append gflags " -recno"
	}
	while { [gets $did str] != -1 && $count < 5 } {
		if { [is_record_based $method] == 1 } {
			global kvals

			set key [expr $count + 1]
			set kvals($key) $str
		} else {
			set key $str
		}
		set ret [eval \
		    {$db put} $txn $pflags {$key [chop_data $method $str]}]
		error_check_good put $ret 0

		set ret [eval {$db get} $gflags {$key}]
		error_check_good \
		    get $ret [list [list $key [pad_data $method $str]]]
		incr count
	}
	close $did
	error_check_good db_close [$db close] 0
	set ret [catch {eval {berkdb_open_noerr -create -mode 0644} $args \
	    {$omethod $testfile $subdb}} db]
	error_check_bad dbopen $ret 0
	#
	# Create a db with no subdbs.  Add no data.  Close.  Try to
	# open/add with a subdb.  Should fail.
	#
	set testfile $testdir/subdb001a.db
	puts "\tSubdb001.a.1: Create db, close, try subdb"
	set db [eval {berkdb_open -create -truncate -mode 0644} $args \
	    {$omethod $testfile}]
	error_check_good dbopen [is_valid_db $db] TRUE
	error_check_good db_close [$db close] 0

	set ret [catch {eval {berkdb_open_noerr -create -mode 0644} $args \
	    {$omethod $testfile $subdb}} db]
	error_check_bad dbopen $ret 0

	if { [is_queue $method] == 1 } {
		puts "Subdb001: skipping remainder of test for method $method"
		return
	}

	#
	# Test naming, db and subdb names beginning with -.
	#
	puts "\tSubdb001.b: Naming"
	set cwd [pwd]
	cd $testdir
	set testfile1 -subdb001.db
	set subdb -subdb
	puts "\tSubdb001.b.0: Create db and subdb with -name, no --"
	set ret [catch {eval {berkdb_open -create -mode 0644} $args \
	    {$omethod $testfile1 $subdb}} db]
	error_check_bad dbopen $ret 0
	puts "\tSubdb001.b.1: Create db and subdb with -name, with --"
	set db [eval {berkdb_open -create -mode 0644} $args \
	    {$omethod -- $testfile1 $subdb}]
	error_check_good dbopen [is_valid_db $db] TRUE
	error_check_good db_close [$db close] 0

	cd $cwd

	#
	# Create 1 db with 1 subdb.  Try to create another subdb of
	# the same name.  Should fail.
	#
	puts "\tSubdb001.c: Existence check"
	set testfile $testdir/subdb001c.db
	set subdb subdb
	set ret [catch {eval {berkdb_open -create -excl -mode 0644} $args \
	    {$omethod $testfile $subdb}} db]
	error_check_good dbopen [is_valid_db $db] TRUE
	set ret [catch {eval {berkdb_open_noerr -create -excl -mode 0644} \
	    $args {$omethod $testfile $subdb}} db1]
	error_check_bad dbopen $ret 0
	error_check_good db_close [$db close] 0

	return
}
