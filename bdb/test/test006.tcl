# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test006.tcl,v 11.13 2000/08/25 14:21:54 sue Exp $
#
# DB Test 6 {access method}
# Keyed delete test.
# Create database.
# Go through database, deleting all entries by key.
proc test006 { method {nentries 10000} {reopen 0} {tnum 6} args} {
	source ./include.tcl

	set do_renumber [is_rrecno $method]
	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { $tnum < 10 } {
		set tname Test00$tnum
		set dbname test00$tnum
	} else {
		set tname Test0$tnum
		set dbname test0$tnum
	}
	puts -nonewline "$tname: $method ($args) "
	puts -nonewline "$nentries equal small key; medium data pairs"
	if {$reopen == 1} {
		puts " (with close)"
	} else {
		puts ""
	}

	# Create the database and open the dictionary
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/$dbname.db
		set env NULL
	} else {
		set testfile $dbname.db
		incr eindex
		set env [lindex $args $eindex]
	}

	set pflags ""
	set gflags ""
	set txn ""
	set count 0
	if { [is_record_based $method] == 1 } {
	   append gflags " -recno"
	}

	# Here is the loop where we put and get each key/data pair

	cleanup $testdir $env
	set db [eval {berkdb_open \
	     -create -truncate -mode 0644} $args {$omethod $testfile}]
	error_check_good dbopen [is_valid_db $db] TRUE

	set did [open $dict]
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [is_record_based $method] == 1 } {
			set key [expr $count + 1 ]
		} else {
			set key $str
		}

		set datastr [make_data_str $str]

		set ret [eval {$db put} \
		    $txn $pflags {$key [chop_data $method $datastr]}]
		error_check_good put $ret 0

		set ret [eval {$db get} $gflags {$key}]
		error_check_good "$tname: put $datastr got $ret" \
		    $ret [list [list $key [pad_data $method $datastr]]]
		incr count
	}
	close $did

	if { $reopen == 1 } {
		error_check_good db_close [$db close] 0

		set db [eval {berkdb_open} $args {$testfile}]
		error_check_good dbopen [is_valid_db $db] TRUE
	}

	# Now we will get each key from the DB and compare the results
	# to the original, then delete it.
	set count 0
	set did [open $dict]
	set key 0
	while { [gets $did str] != -1 && $count < $nentries } {
		if { $do_renumber == 1 } {
			set key 1
		} elseif { [is_record_based $method] == 1 } {
			incr key
		} else {
			set key $str
		}

		set datastr [make_data_str $str]

		set ret [eval {$db get} $gflags {$key}]
		error_check_good "$tname: get $datastr got $ret" \
		    $ret [list [list $key [pad_data $method $datastr]]]

		set ret [eval {$db del} $txn {$key}]
		error_check_good db_del:$key $ret 0
		incr count
	}
	close $did

	error_check_good db_close [$db close] 0
}
