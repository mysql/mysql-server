# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: sdb007.tcl,v 11.13 2000/12/11 17:24:55 sue Exp $
#
# Sub DB Test 7 {access method}
# Use the first 10,000 entries from the dictionary spread across each subdb.
# Use a different page size for every subdb.
# Insert each with self as key and data; retrieve each.
# After all are entered, retrieve all; compare output to original.
# Close file, reopen, do retrieve and re-verify.
proc subdb007 { method {nentries 10000} args } {
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_queue $method] == 1 } {
		puts "Subdb007: skipping for method $method"
		return
	}
	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Subdb007: skipping for specific pagesizes"
		return
	}

	puts "Subdb007: $method ($args) subdb tests with different pagesizes"

	# Create the database and open the dictionary
	set testfile $testdir/subdb007.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	set t4 $testdir/t4
	cleanup $testdir NULL

	set txn ""
	set count 0

	if { [is_record_based $method] == 1 } {
		set checkfunc subdb007_recno.check
	} else {
		set checkfunc subdb007.check
	}
	puts "\tSubdb007.a: create subdbs of different page sizes"
	set psize {8192 4096 2048 1024 512}
	set nsubdbs [llength $psize]
	for { set i 0 } { $i < $nsubdbs } { incr i } {
		lappend duplist -1
	}
	set newent [expr $nentries / $nsubdbs]
	build_all_subdb $testfile [list $method] $psize $duplist $newent $args

	# Now we will get each key from the DB and compare the results
	# to the original.
	for { set subdb 0 } { $subdb < $nsubdbs } { incr subdb } {
		puts "\tSubdb007.b: dump file sub$subdb.db"
		set db [berkdb_open -unknown $testfile sub$subdb.db]
		dump_file $db $txn $t1 $checkfunc
		error_check_good db_close [$db close] 0

		# Now compare the keys to see if they match the dictionary
		# (or ints)
		if { [is_record_based $method] == 1 } {
			set oid [open $t2 w]
			for {set i 1} {$i <= $newent} {incr i} {
				puts $oid [expr $subdb * $newent + $i]
			}
			close $oid
			file rename -force $t1 $t3
		} else {
			set beg [expr $subdb * $newent]
			incr beg
			set end [expr $beg + $newent - 1]
			filehead $end $dict $t3 $beg
			filesort $t3 $t2
			filesort $t1 $t3
		}

		error_check_good Subdb007:diff($t3,$t2) \
		    [filecmp $t3 $t2] 0

		puts "\tSubdb007.c: sub$subdb.db: close, open, and dump file"
		# Now, reopen the file and run the last test again.
		open_and_dump_subfile $testfile NULL $txn $t1 $checkfunc \
		    dump_file_direction "-first" "-next" sub$subdb.db
		if { [is_record_based $method] != 1 } {
			filesort $t1 $t3
		}

		error_check_good Subdb007:diff($t2,$t3) \
		    [filecmp $t2 $t3] 0

		# Now, reopen the file and run the last test again in the
		# reverse direction.
		puts "\tSubdb007.d: sub$subdb.db:\
		    close, open, and dump file in reverse direction"
		open_and_dump_subfile $testfile NULL $txn $t1 $checkfunc \
		    dump_file_direction "-last" "-prev" sub$subdb.db

		if { [is_record_based $method] != 1 } {
			filesort $t1 $t3
		}

		error_check_good Subdb007:diff($t3,$t2) \
		    [filecmp $t3 $t2] 0
	}
}

# Check function for Subdb007; keys and data are identical
proc subdb007.check { key data } {
	error_check_good "key/data mismatch" $data $key
}

proc subdb007_recno.check { key data } {
global dict
global kvals
	error_check_good key"$key"_exists [info exists kvals($key)] 1
	error_check_good "key/data mismatch, key $key" $data $kvals($key)
}
