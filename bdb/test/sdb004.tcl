# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: sdb004.tcl,v 11.14 2000/08/25 14:21:53 sue Exp $
#
# SubDB Test 4 {access method}
# Create 1 db with many large subdbs.  Use the contents as subdb names.
# Take the source files and dbtest executable and enter their names as the
# key with their contents as data.  After all are entered, retrieve all;
# compare output to original. Close file, reopen, do retrieve and re-verify.
proc subdb004 { method args} {
	global names
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_queue $method] == 1 || [is_fixed_length $method] == 1 } {
		puts "Subdb004: skipping for method $method"
		return
	}

	puts "Subdb004: $method ($args) \
	    filecontents=subdbname filename=key filecontents=data pairs"

	# Create the database and open the dictionary
	set testfile $testdir/subdb004.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	set t4 $testdir/t4

	cleanup $testdir NULL
	set pflags ""
	set gflags ""
	set txn ""
	if { [is_record_based $method] == 1 } {
		set checkfunc subdb004_recno.check
		append gflags "-recno"
	} else {
		set checkfunc subdb004.check
	}

	# Here is the loop where we put and get each key/data pair
	set file_list [glob ../*/*.c ./libdb.so.3.0 ./libtool ./libtool.exe]
	set fcount [llength $file_list]

	set count 0
	if { [is_record_based $method] == 1 } {
		set oid [open $t2 w]
		for {set i 1} {$i <= $fcount} {set i [incr i]} {
			puts $oid $i
		}
		close $oid
	} else {
		set oid [open $t2.tmp w]
		foreach f $file_list {
			puts $oid $f
		}
		close $oid
		filesort $t2.tmp $t2
	}
	puts "\tSubdb004.a: Set/Check each subdb"
	foreach f $file_list {
		if { [is_record_based $method] == 1 } {
			set key [expr $count + 1]
			set names([expr $count + 1]) $f
		} else {
			set key $f
		}
		# Should really catch errors
		set fid [open $f r]
		fconfigure $fid -translation binary
		set data [read $fid]
		set subdb $data
		close $fid
		set db [eval {berkdb_open -create -mode 0644} \
		    $args {$omethod $testfile $subdb}]
		error_check_good dbopen [is_valid_db $db] TRUE
		set ret [eval \
		    {$db put} $txn $pflags {$key [chop_data $method $data]}]
		error_check_good put $ret 0

		# Should really catch errors
		set fid [open $t4 w]
		fconfigure $fid -translation binary
		if [catch {eval {$db get} $gflags {$key}} data] {
			puts -nonewline $fid $data
		} else {
			# Data looks like {{key data}}
			set key [lindex [lindex $data 0] 0]
			set data [lindex [lindex $data 0] 1]
			puts -nonewline $fid $data
		}
		close $fid

		error_check_good Subdb004:diff($f,$t4) \
		    [filecmp $f $t4] 0

		incr count

		# Now we will get each key from the DB and compare the results
		# to the original.
		# puts "\tSubdb004.b: dump file"
		dump_bin_file $db $txn $t1 $checkfunc
		error_check_good db_close [$db close] 0

	}

	#
	# Now for each file, check that the subdb name is the same
	# as the data in that subdb and that the filename is the key.
	#
	puts "\tSubdb004.b: Compare subdb names with key/data"
	set db [berkdb_open -rdonly $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE
	set c [eval {$db cursor} $txn]
	error_check_good db_cursor [is_valid_cursor $c $db] TRUE

	for {set d [$c get -first] } { [llength $d] != 0 } \
	    {set d [$c get -next] } {
		set subdbname [lindex [lindex $d 0] 0]
		set subdb [berkdb_open $testfile $subdbname]
		error_check_good dbopen [is_valid_db $db] TRUE

		# Output the subdb name
		set ofid [open $t3 w]
		fconfigure $ofid -translation binary
		set subdbname [string trimright $subdbname \0]
		puts -nonewline $ofid $subdbname
		close $ofid

		# Output the data
		set subc [eval {$subdb cursor} $txn]
		error_check_good db_cursor [is_valid_cursor $subc $subdb] TRUE
		set d [$subc get -first]
		error_check_good dbc_get [expr [llength $d] != 0] 1
		set key [lindex [lindex $d 0] 0]
		set data [lindex [lindex $d 0] 1]

		set ofid [open $t1 w]
		fconfigure $ofid -translation binary
		puts -nonewline $ofid $data
		close $ofid

		$checkfunc $key $t1
		$checkfunc $key $t3

		error_check_good Subdb004:diff($t3,$t1) \
		    [filecmp $t3 $t1] 0
		error_check_good curs_close [$subc close] 0
		error_check_good db_close [$subdb close] 0
	}
	error_check_good curs_close [$c close] 0
	error_check_good db_close [$db close] 0

	if { [is_record_based $method] != 1 } {
		fileremove $t2.tmp
	}
}

# Check function for subdb004; key should be file name; data should be contents
proc subdb004.check { binfile tmpfile } {
	source ./include.tcl

	error_check_good Subdb004:datamismatch($binfile,$tmpfile) \
	    [filecmp $binfile $tmpfile] 0
}
proc subdb004_recno.check { binfile tmpfile } {
	global names
	source ./include.tcl

	set fname $names($binfile)
	error_check_good key"$binfile"_exists [info exists names($binfile)] 1
	error_check_good Subdb004:datamismatch($fname,$tmpfile) \
	    [filecmp $fname $tmpfile] 0
}
