# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test029.tcl,v 11.13 2000/08/25 14:21:55 sue Exp $
#
# DB Test 29 {method nentries}
# Test the Btree and Record number renumbering.
proc test029 { method {nentries 10000} args} {
	source ./include.tcl

	set do_renumber [is_rrecno $method]
	set args [convert_args $method $args]
	set omethod [convert_method $method]

	puts "Test029: $method ($args)"

	if { [string compare $omethod "-hash"] == 0 } {
		puts "Test029 skipping for method HASH"
		return
	}
	if { [is_record_based $method] == 1 && $do_renumber != 1 } {
		puts "Test029 skipping for method RECNO (w/out renumbering)"
		return
	}

	# Create the database and open the dictionary
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test029.db
		set env NULL
	} else {
		set testfile test029.db
		incr eindex
		set env [lindex $args $eindex]
	}
	cleanup $testdir $env

	# Read the first nentries dictionary elements and reverse them.
	# Keep a list of these (these will be the keys).
	puts "\tTest029.a: initialization"
	set keys ""
	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $nentries } {
		lappend keys [reverse $str]
		incr count
	}
	close $did

	# Generate sorted order for the keys
	set sorted_keys [lsort $keys]

	# Save the first and last keys
	set last_key [lindex $sorted_keys end]
	set last_keynum [llength $sorted_keys]

	set first_key [lindex $sorted_keys 0]
	set first_keynum 1

	# Create the database
	if { [string compare $omethod "-btree"] == 0 } {
		set db [eval {berkdb_open -create -truncate \
			-mode 0644 -recnum} $args {$omethod $testfile}]
	   error_check_good dbopen [is_valid_db $db] TRUE
	} else {
		set db [eval {berkdb_open -create -truncate \
			-mode 0644} $args {$omethod $testfile}]
	   error_check_good dbopen [is_valid_db $db] TRUE
	}

	set pflags ""
	set gflags ""
	set txn ""

	if { [is_record_based $method] == 1 } {
		append gflags " -recno"
	}

	puts "\tTest029.b: put/get loop"
	foreach k $keys {
		if { [is_record_based $method] == 1 } {
			set key [lsearch $sorted_keys $k]
			incr key
		} else {
			set key $k
		}
		set ret [eval {$db put} \
		    $txn $pflags {$key [chop_data $method $k]}]
		error_check_good dbput $ret 0

		set ret [eval {$db get} $txn $gflags {$key}]
		if { [string compare [lindex [lindex $ret 0] 1] $k] != 0 } {
			puts "Test029: put key-data $key $k got $ret"
			return
		}
	}

	# Now delete the first key in the database
	puts "\tTest029.c: delete and verify renumber"

	# Delete the first key in the file
	if { [is_record_based $method] == 1 } {
		set key $first_keynum
	} else {
		set key $first_key
	}

	set ret [eval {$db del} $txn {$key}]
	error_check_good db_del $ret 0

	# Now we are ready to retrieve records based on
	# record number
	if { [string compare $omethod "-btree"] == 0 } {
		append gflags " -recno"
	}

	# First try to get the old last key (shouldn't exist)
	set ret [eval {$db get} $txn $gflags {$last_keynum}]
	error_check_good get_after_del $ret [list]

	# Now try to get what we think should be the last key
	set ret [eval {$db get} $txn $gflags {[expr $last_keynum - 1]}]
	error_check_good \
	    getn_last_after_del [lindex [lindex $ret 0] 1] $last_key

	# Create a cursor; we need it for the next test and we
	# need it for recno here.
	set dbc [eval {$db cursor} $txn]
	error_check_good db_cursor [is_substr $dbc $db] 1

	# OK, now re-put the first key and make sure that we
	# renumber the last key appropriately.
	if { [string compare $omethod "-btree"] == 0 } {
	set ret [eval {$db put} $txn {$key [chop_data $method $first_key]}]
		error_check_good db_put $ret 0
	} else {
		# Recno
		set ret [eval {$dbc get} $txn {-first}]
		set ret [eval {$dbc put} $txn $pflags {-before $first_key}]
		error_check_bad dbc_put:DB_BEFORE $ret 0
	}

	# Now check that the last record matches the last record number
	set ret [eval {$db get} $txn $gflags {$last_keynum}]
	error_check_good \
	    getn_last_after_put [lindex [lindex $ret 0] 1] $last_key

	# Now delete the first key in the database using a cursor
	puts "\tTest029.d: delete with cursor and verify renumber"

	set ret [eval {$dbc get} $txn {-first}]
	error_check_good dbc_first $ret [list [list $key $first_key]]

	# Now delete at the cursor
	set ret [$dbc del]
	error_check_good dbc_del $ret 0

	# Now check the record numbers of the last keys again.
	# First try to get the old last key (shouldn't exist)
	set ret [eval {$db get} $txn $gflags {$last_keynum}]
	error_check_good get_last_after_cursor_del:$ret $ret [list]

	# Now try to get what we think should be the last key
	set ret [eval {$db get} $txn $gflags {[expr $last_keynum - 1]}]
	error_check_good \
	    getn_after_cursor_del [lindex [lindex $ret 0] 1] $last_key

	# Re-put the first key and make sure that we renumber the last
	# key appropriately.
	puts "\tTest029.e: put with cursor and verify renumber"
	if { [string compare $omethod "-btree"] == 0 } {
		set ret [eval {$dbc put} \
		    $txn $pflags {-current $first_key}]
		error_check_good dbc_put:DB_CURRENT $ret 0
	} else {
		set ret [eval {$dbc put} $txn $pflags {-before $first_key}]
		error_check_bad dbc_put:DB_BEFORE $ret 0
	}

	# Now check that the last record matches the last record number
	set ret [eval {$db get} $txn $gflags {$last_keynum}]
	error_check_good \
	    get_after_cursor_reput [lindex [lindex $ret 0] 1] $last_key

	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0
}
