# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: sdb008.tcl,v 11.14 2000/08/25 14:21:53 sue Exp $
#
# Sub DB Test 8 {access method}
# Use the first 10,000 entries from the dictionary.
# Use a different or random lorder for each subdb.
# Insert each with self as key and data; retrieve each.
# After all are entered, retrieve all; compare output to original.
# Close file, reopen, do retrieve and re-verify.
proc subdb008 { method {nentries 10000} args } {
	source ./include.tcl
	global rand_init

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_queue $method] == 1 } {
		puts "Subdb008: skipping for method $method"
		return
	}

	berkdb srand $rand_init

	puts "Subdb008: $method ($args) subdb lorder tests"

	# Create the database and open the dictionary
	set testfile $testdir/subdb008.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	set t4 $testdir/t4
	cleanup $testdir NULL

	set txn ""
	set pflags ""
	set gflags ""

	if { [is_record_based $method] == 1 } {
		set checkfunc subdb008_recno.check
	} else {
		set checkfunc subdb008.check
	}
	set nsubdbs 4
	set lo [list 4321 1234]
	puts "\tSubdb008.a: put/get loop"
	# Here is the loop where we put and get each key/data pair
	for { set i 0 } { $i < $nsubdbs } { incr i } {
		set subdb sub$i.db
		if { $i >= [llength $lo]} {
			set r [berkdb random_int 0 1]
			set order [lindex $lo $r]
		} else {
			set order [lindex $lo $i]
		}
		set db [eval {berkdb_open -create -mode 0644} \
		    $args {-lorder $order $omethod $testfile $subdb}]
		set did [open $dict]
		set count 0
		while { [gets $did str] != -1 && $count < $nentries } {
			if { [is_record_based $method] == 1 } {
				global kvals

				set gflags "-recno"
				set key [expr $i * $nentries]
				set key [expr $key + $count + 1]
				set kvals($key) [pad_data $method $str]
			} else {
				set key $str
			}
			set ret [eval {$db put} \
			    $txn $pflags {$key [chop_data $method $str]}]
			error_check_good put $ret 0

			set ret [eval {$db get} $gflags {$key}]
			error_check_good \
			    get $ret [list [list $key [pad_data $method $str]]]
			incr count
		}
		close $did
		error_check_good db_close [$db close] 0
	}

	# Now we will get each key from the DB and compare the results
	# to the original.
	for { set subdb 0 } { $subdb < $nsubdbs } { incr subdb } {
		puts "\tSubdb008.b: dump file sub$subdb.db"
		set db [berkdb_open -unknown $testfile sub$subdb.db]
		dump_file $db $txn $t1 $checkfunc
		error_check_good db_close [$db close] 0

		# Now compare the keys to see if they match the dictionary
		# (or ints)
		if { [is_record_based $method] == 1 } {
			set oid [open $t2 w]
			for {set i 1} {$i <= $nentries} {incr i} {
				puts $oid [expr $subdb * $nentries + $i]
			}
			close $oid
			file rename -force $t1 $t3
		} else {
			set q q
			filehead $nentries $dict $t3
			filesort $t3 $t2
			filesort $t1 $t3
		}

		error_check_good Subdb008:diff($t3,$t2) \
		    [filecmp $t3 $t2] 0

		puts "\tSubdb008.c: sub$subdb.db: close, open, and dump file"
		# Now, reopen the file and run the last test again.
		open_and_dump_subfile $testfile NULL $txn $t1 $checkfunc \
		    dump_file_direction "-first" "-next" sub$subdb.db
		if { [is_record_based $method] != 1 } {
			filesort $t1 $t3
		}

		error_check_good Subdb008:diff($t2,$t3) \
		    [filecmp $t2 $t3] 0

		# Now, reopen the file and run the last test again in the
		# reverse direction.
		puts "\tSubdb008.d: sub$subdb.db:\
		    close, open, and dump file in reverse direction"
		open_and_dump_subfile $testfile NULL $txn $t1 $checkfunc \
		    dump_file_direction "-last" "-prev" sub$subdb.db

		if { [is_record_based $method] != 1 } {
			filesort $t1 $t3
		}

		error_check_good Subdb008:diff($t3,$t2) \
		    [filecmp $t3 $t2] 0
	}
}

# Check function for Subdb008; keys and data are identical
proc subdb008.check { key data } {
	error_check_good "key/data mismatch" $data $key
}

proc subdb008_recno.check { key data } {
global dict
global kvals
	error_check_good key"$key"_exists [info exists kvals($key)] 1
	error_check_good "key/data mismatch, key $key" $data $kvals($key)
}
