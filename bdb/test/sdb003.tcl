# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: sdb003.tcl,v 11.17 2000/08/25 14:21:52 sue Exp $
#
# Sub DB Test 3 {access method}
# Use the first 10,000 entries from the dictionary as subdbnames.
# Insert each with entry as name of subdatabase and a partial list as key/data.
# After all are entered, retrieve all; compare output to original.
# Close file, reopen, do retrieve and re-verify.
proc subdb003 { method {nentries 1000} args } {
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_queue $method] == 1 } {
		puts "Subdb003: skipping for method $method"
		return
	}

	puts "Subdb003: $method ($args) many subdb tests"

	# Create the database and open the dictionary
	set testfile $testdir/subdb003.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir NULL

	set pflags ""
	set gflags ""
	set txn ""
	set fcount 0

	if { [is_record_based $method] == 1 } {
		set checkfunc subdb003_recno.check
		append gflags " -recno"
	} else {
		set checkfunc subdb003.check
	}

	# Here is the loop where we put and get each key/data pair
	set ndataent 10
	set fdid [open $dict]
	while { [gets $fdid str] != -1 && $fcount < $nentries } {
		set subdb $str
		set db [eval {berkdb_open -create -mode 0644} \
		    $args {$omethod $testfile $subdb}]
		error_check_good dbopen [is_valid_db $db] TRUE

		set count 0
		set did [open $dict]
		while { [gets $did str] != -1 && $count < $ndataent } {
			if { [is_record_based $method] == 1 } {
				global kvals

				set key [expr $count + 1]
				set kvals($key) [pad_data $method $str]
			} else {
				set key $str
			}
			set ret [eval {$db put} \
			    $txn $pflags {$key [chop_data $method $str]}]
			error_check_good put $ret 0

			set ret [eval {$db get} $gflags {$key}]
			error_check_good get $ret [list [list $key [pad_data $method $str]]]
			incr count
		}
		close $did
		incr fcount

		dump_file $db $txn $t1 $checkfunc
		error_check_good db_close [$db close] 0

		# Now compare the keys to see if they match
		if { [is_record_based $method] == 1 } {
			set oid [open $t2 w]
			for {set i 1} {$i <= $ndataent} {set i [incr i]} {
				puts $oid $i
			}
			close $oid
			file rename -force $t1 $t3
		} else {
			set q q
			filehead $ndataent $dict $t3
			filesort $t3 $t2
			filesort $t1 $t3
		}

		error_check_good Subdb003:diff($t3,$t2) \
		    [filecmp $t3 $t2] 0

		# Now, reopen the file and run the last test again.
		open_and_dump_subfile $testfile NULL $txn $t1 $checkfunc \
		dump_file_direction "-first" "-next" $subdb
		if { [is_record_based $method] != 1 } {
			filesort $t1 $t3
		}

		error_check_good Subdb003:diff($t2,$t3) \
		    [filecmp $t2 $t3] 0

		# Now, reopen the file and run the last test again in the
		# reverse direction.
		open_and_dump_subfile $testfile NULL $txn $t1 $checkfunc \
		    dump_file_direction "-last" "-prev" $subdb

		if { [is_record_based $method] != 1 } {
			filesort $t1 $t3
		}

		error_check_good Subdb003:diff($t3,$t2) \
		    [filecmp $t3 $t2] 0
		if { [expr $fcount % 100] == 0 } {
			puts -nonewline "$fcount "
			flush stdout
		}
	}
	puts ""
}

# Check function for Subdb003; keys and data are identical
proc subdb003.check { key data } {
	error_check_good "key/data mismatch" $data $key
}

proc subdb003_recno.check { key data } {
	global dict
	global kvals

	error_check_good key"$key"_exists [info exists kvals($key)] 1
	error_check_good "key/data mismatch, key $key" $data $kvals($key)
}
