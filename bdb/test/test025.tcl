# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test025.tcl,v 11.11 2000/11/16 23:56:18 ubell Exp $
#
# DB Test 25 {method nentries}
# Test the DB_APPEND flag.
proc test025 { method {nentries 10000} {start 0 } {tnum "25" } args} {
	global kvals
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]
	puts "Test0$tnum: $method ($args)"

	if { [string compare $omethod "-btree"] == 0 } {
		puts "Test0$tnum skipping for method BTREE"
		return
	}
	if { [string compare $omethod "-hash"] == 0 } {
		puts "Test0$tnum skipping for method HASH"
		return
	}

	# Create the database and open the dictionary
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
	set t1 $testdir/t1

	cleanup $testdir $env
	set db [eval {berkdb_open \
	     -create -truncate -mode 0644} $args {$omethod $testfile}]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]

	puts "\tTest0$tnum.a: put/get loop"
	set gflags " -recno"
	set pflags " -append"
	set txn ""
	set checkfunc test025_check

	# Here is the loop where we put and get each key/data pair
	set count $start
	set nentries [expr $start + $nentries]
	if { $count != 0 } {
		gets $did str
		set k [expr $count + 1]
		set kvals($k) [pad_data $method $str]
		set ret [eval {$db put} $txn $k {[chop_data $method $str]}]
		error_check_good db_put $ret 0
		incr count
	}
		
	while { [gets $did str] != -1 && $count < $nentries } {
		set k [expr $count + 1]
		set kvals($k) [pad_data $method $str]
		set ret [eval {$db put} $txn $pflags {[chop_data $method $str]}]
		error_check_good db_put $ret $k

		set ret [eval {$db get} $txn $gflags {$k}]
		error_check_good \
		    get $ret [list [list $k [pad_data $method $str]]]
		incr count
		if { [expr $count + 1] == 0 } {
			incr count
		}
	}
	close $did

	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest0$tnum.b: dump file"
	dump_file $db $txn $t1 $checkfunc
	error_check_good db_close [$db close] 0

	puts "\tTest0$tnum.c: close, open, and dump file"
	# Now, reopen the file and run the last test again.
	open_and_dump_file $testfile $env $txn $t1 $checkfunc \
	    dump_file_direction -first -next

	# Now, reopen the file and run the last test again in the
	# reverse direction.
	puts "\tTest0$tnum.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile $env $txn $t1 $checkfunc \
		dump_file_direction -last -prev
}

proc test025_check { key data } {
	global kvals

	error_check_good key"$key"_exists [info exists kvals($key)] 1
	error_check_good " key/data mismatch for |$key|" $data $kvals($key)
}
