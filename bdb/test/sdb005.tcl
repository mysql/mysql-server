# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: sdb005.tcl,v 11.12 2000/08/25 14:21:53 sue Exp $
#
# Test cursor operations between subdbs.
#
# We should test this on all btrees, all hash, and a combination thereof
proc subdb005 {method {nentries 100} args } {
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_queue $method] == 1 } {
		puts "Subdb005: skipping for method $method"
		return
	}

	puts "Subdb005: $method ( $args ) subdb cursor operations test"
	set txn ""
	cleanup $testdir NULL
	set psize 8192
	set testfile $testdir/subdb005.db
	set duplist {-1 -1 -1 -1 -1}
	build_all_subdb \
	    $testfile [list $method] [list $psize] $duplist $nentries $args
	set numdb [llength $duplist]
	#
	# Get a cursor in each subdb and move past the end of each
	# subdb.  Make sure we don't end up in another subdb.
	#
	puts "\tSubdb005.a: Cursor ops - first/prev and last/next"
	for {set i 0} {$i < $numdb} {incr i} {
		set db [berkdb_open -unknown $testfile sub$i.db]
		error_check_good dbopen [is_valid_db $db] TRUE
		set db_handle($i) $db
		# Used in 005.c test
		lappend subdbnames sub$i.db

		set dbc [eval {$db cursor} $txn]
		error_check_good db_cursor [is_valid_cursor $dbc $db] TRUE
		set d [$dbc get -first]
		error_check_good dbc_get [expr [llength $d] != 0] 1

		# Used in 005.b test
		set db_key($i) [lindex [lindex $d 0] 0]

		set d [$dbc get -prev]
		error_check_good dbc_get [expr [llength $d] == 0] 1
		set d [$dbc get -last]
		error_check_good dbc_get [expr [llength $d] != 0] 1
		set d [$dbc get -next]
		error_check_good dbc_get [expr [llength $d] == 0] 1
	}
	#
	# Get a key from each subdb and try to get this key in a
	# different subdb.  Make sure it fails
	#
	puts "\tSubdb005.b: Get keys in different subdb's"
	for {set i 0} {$i < $numdb} {incr i} {
		set n [expr $i + 1]
		if {$n == $numdb} {
			set n 0
		}
		set db $db_handle($i)
		if { [is_record_based $method] == 1 } {
			set d [$db get -recno $db_key($n)]
			error_check_good \
			    db_get [expr [llength $d] == 0] 1
		} else {
			set d [$db get $db_key($n)]
			error_check_good db_get [expr [llength $d] == 0] 1
		}
	}

	#
	# Clean up
	#
	for {set i 0} {$i < $numdb} {incr i} {
		error_check_good db_close [$db_handle($i) close] 0
	}

	#
	# Check contents of DB for subdb names only.  Makes sure that
	# every subdbname is there and that nothing else is there.
	#
	puts "\tSubdb005.c: Check DB is read-only"
	error_check_bad dbopen [catch \
	     {berkdb_open_noerr -unknown $testfile} ret] 0

	puts "\tSubdb005.d: Check contents of DB for subdb names only"
	set db [berkdb_open -unknown -rdonly $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE
	set subdblist [$db get -glob *]
	foreach kd $subdblist {
		# subname also used in subdb005.e,f below
		set subname [lindex $kd 0]
		set i [lsearch $subdbnames $subname]
		error_check_good subdb_search [expr $i != -1] 1
		set subdbnames [lreplace $subdbnames $i $i]
	}
	error_check_good subdb_done [llength $subdbnames] 0

	error_check_good db_close [$db close] 0
	return
}
