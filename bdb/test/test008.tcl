# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test008.tcl,v 11.17 2000/10/19 17:35:39 sue Exp $
#
# DB Test 8 {access method}
# Take the source files and dbtest executable and enter their names as the
# key with their contents as data.  After all are entered, begin looping
# through the entries; deleting some pairs and then readding them.
proc test008 { method {nentries 10000} {reopen 8} {debug 0} args} {
	source ./include.tcl

	set tnum test00$reopen
	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_record_based $method] == 1 } {
		puts "Test00$reopen skipping for method $method"
		return
	}

	puts -nonewline "$tnum: $method filename=key filecontents=data pairs"
	if {$reopen == 9} {
		puts "(with close)"
	} else {
		puts ""
	}

	# Create the database and open the dictionary
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/$tnum.db
		set env NULL
	} else {
		set testfile $tnum.db
		incr eindex
		set env [lindex $args $eindex]
	}
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	set t4 $testdir/t4

	cleanup $testdir $env

	set db [eval {berkdb_open -create -truncate -mode 0644} \
	    $args {$omethod $testfile}]
	error_check_good dbopen [is_valid_db $db] TRUE

	set pflags ""
	set gflags ""
	set txn ""

	# Here is the loop where we put and get each key/data pair
	set file_list [glob ../*/*.c ./*.o ./*.lo ./*.exe]

	set count 0
	puts "\tTest00$reopen.a: Initial put/get loop"
	foreach f $file_list {
		set names($count) $f
		set key $f

		put_file $db $txn $pflags $f

		get_file $db $txn $gflags $f $t4

		error_check_good Test00$reopen:diff($f,$t4) \
		    [filecmp $f $t4] 0

		incr count
	}

	if {$reopen == 9} {
		error_check_good db_close [$db close] 0

		set db [eval {berkdb_open} $args $testfile]
		error_check_good dbopen [is_valid_db $db] TRUE
	}

	# Now we will get step through keys again (by increments) and
	# delete all the entries, then re-insert them.

	puts "\tTest00$reopen.b: Delete re-add loop"
	foreach i "1 2 4 8 16" {
		for {set ndx 0} {$ndx < $count} { incr ndx $i} {
			set r [eval {$db del} $txn {$names($ndx)}]
			error_check_good db_del:$names($ndx) $r 0
		}
		for {set ndx 0} {$ndx < $count} { incr ndx $i} {
			put_file $db $txn $pflags $names($ndx)
		}
	}

	if {$reopen == 9} {
		error_check_good db_close [$db close] 0
		set db [eval {berkdb_open} $args $testfile]
		error_check_good dbopen [is_valid_db $db] TRUE
	}

	# Now, reopen the file and make sure the key/data pairs look right.
	puts "\tTest00$reopen.c: Dump contents forward"
	dump_bin_file $db $txn $t1 test008.check

	set oid [open $t2.tmp w]
	foreach f $file_list {
		puts $oid $f
	}
	close $oid
	filesort $t2.tmp $t2
	fileremove $t2.tmp
	filesort $t1 $t3

	error_check_good Test00$reopen:diff($t3,$t2) \
	    [filecmp $t3 $t2] 0

	# Now, reopen the file and run the last test again in reverse direction.
	puts "\tTest00$reopen.d: Dump contents backward"
	dump_bin_file_direction $db $txn $t1 test008.check "-last" "-prev"

	filesort $t1 $t3

	error_check_good Test00$reopen:diff($t3,$t2) \
	    [filecmp $t3 $t2] 0
	error_check_good close:$db [$db close] 0
}

proc test008.check { binfile tmpfile } {
	global tnum
	source ./include.tcl

	error_check_good diff($binfile,$tmpfile) \
	    [filecmp $binfile $tmpfile] 0
}
