# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test004.tcl,v 11.15 2000/08/25 14:21:54 sue Exp $
#
# DB Test 4 {access method}
# Check that cursor operations work.  Create a database.
# Read through the database sequentially using cursors and
# delete each element.
proc test004 { method {nentries 10000} {reopen 4} {build_only 0} args} {
	source ./include.tcl

	set do_renumber [is_rrecno $method]
	set args [convert_args $method $args]
	set omethod [convert_method $method]

	set tnum test00$reopen

	puts -nonewline "$tnum:\
	    $method ($args) $nentries delete small key; medium data pairs"
	if {$reopen == 5} {
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
		set testfile $testdir/test004.db
		set env NULL
	} else {
		set testfile test004.db
		incr eindex
		set env [lindex $args $eindex]
	}
	# Create the database and open the dictionary
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir $env
	set db [eval {berkdb_open -create -truncate -mode 0644} $args {$omethod $testfile}]
	error_check_good dbopen [is_valid_db $db] TRUE

	set did [open $dict]

	set pflags ""
	set gflags ""
	set txn ""
	set count 0

	if { [is_record_based $method] == 1 } {
		append gflags " -recno"
	}

	# Here is the loop where we put and get each key/data pair
	set kvals ""
	puts "\tTest00$reopen.a: put/get loop"
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [is_record_based $method] == 1 } {
			set key [expr $count + 1]
			lappend kvals $str
		} else {
			set key $str
		}

		set datastr [ make_data_str $str ]

		set ret [eval {$db put} $txn $pflags {$key [chop_data $method $datastr]}]
		error_check_good put $ret 0

		set ret [eval {$db get} $gflags {$key}]
		error_check_good "$tnum:put" $ret \
		    [list [list $key [pad_data $method $datastr]]]
		incr count
	}
	close $did
	if { $build_only == 1 } {
		return $db
	}
	if { $reopen == 5 } {
		error_check_good db_close [$db close] 0

		set db [eval {berkdb_open} $args {$testfile}]
		error_check_good dbopen [is_valid_db $db] TRUE
	}
	puts "\tTest00$reopen.b: get/delete loop"
	# Now we will get each key from the DB and compare the results
	# to the original, then delete it.
	set outf [open $t1 w]
	set c [eval {$db cursor} $txn]

	set count 0
	for {set d [$c get -first] } { [llength $d] != 0 } {
	    set d [$c get -next] } {
		set k [lindex [lindex $d 0] 0]
		set d2 [lindex [lindex $d 0] 1]
		if { [is_record_based $method] == 1 } {
			set datastr \
			    [make_data_str [lindex $kvals [expr $k - 1]]]
		} else {
			set datastr [make_data_str $k]
		}
		error_check_good $tnum:$k $d2 [pad_data $method $datastr]
		puts $outf $k
		$c del
		if { [is_record_based $method] == 1 && \
			$do_renumber == 1 } {
			set kvals [lreplace $kvals 0 0]
		}
		incr count
	}
	close $outf
	error_check_good curs_close [$c close] 0

	# Now compare the keys to see if they match the dictionary
	if { [is_record_based $method] == 1 } {
		error_check_good test00$reopen:keys_deleted $count $nentries
	} else {
		set q q
		filehead $nentries $dict $t3
		filesort $t3 $t2
		filesort $t1 $t3
		error_check_good Test00$reopen:diff($t3,$t2) \
		    [filecmp $t3 $t2] 0
	}

	error_check_good db_close [$db close] 0
}
