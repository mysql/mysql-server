# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test067.tcl,v 11.12 2000/08/25 14:21:58 sue Exp $
#
# DB Test 67: Test of DB_CURRENT partial puts on almost-empty duplicate pages.
# This test was written to address the following issue, #2 in the list of
# issues relating to bug #0820:
#   2. DBcursor->put, DB_CURRENT flag, off-page duplicates, hash and btree:
#	In Btree, the DB_CURRENT overwrite of off-page duplicate records
#	first deletes the record and then puts the new one -- this could
#	be a problem if the removal of the record causes a reverse split.
#	Suggested solution is to acquire a cursor to lock down the current
#	record, put a new record after that record, and then delete using
#	the held cursor.
# It also tests the following, #5 in the same list of issues:
#  5. DBcursor->put, DB_AFTER/DB_BEFORE/DB_CURRENT flags, DB_DBT_PARTIAL set,
#     duplicate comparison routine specified.
#	The partial change does not change how data items sort, but the
#	record to be put isn't built yet, and that record supplied is the
#	one that's checked for ordering compatibility.
proc test067 { method {ndups 1000} {tnum 67} args } {
	source ./include.tcl
	global alphabet
	global errorCode

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	set eindex [lsearch -exact $args "-env"]

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

	puts "Test0$tnum:\
	    $method ($args) Partial puts on near-empty duplicate pages."
	if { [is_record_based $method] == 1 || [is_rbtree $method] == 1 } {
	    puts "\tTest0$tnum: skipping for method $method."
	    return
	}

	foreach dupopt { "-dup" "-dup -dupsort" } {
		cleanup $testdir $env
		set db [eval {berkdb_open -create -truncate -mode 0644 \
		    $omethod} $args $dupopt {$testfile}]
		error_check_good db_open [is_valid_db $db] TRUE

		puts "\tTest0$tnum.a ($dupopt): Put $ndups duplicates."

		set key "key_test$tnum"

		for { set ndx 0 } { $ndx < $ndups } { incr ndx } {
			set data $alphabet$ndx

			# No need for pad_data since we're skipping recno.
			set ret [eval {$db put} $key $data]
			error_check_good put($key,$data) $ret 0
		}

		# Sync so we can inspect database if the next section bombs.
		error_check_good db_sync [$db sync] 0
		puts "\tTest0$tnum.b ($dupopt):\
		    Deleting dups (last first), overwriting each."

		set dbc [$db cursor]
		error_check_good cursor_create [is_valid_cursor $dbc $db] TRUE

		set count 0
		while { $count < $ndups - 1 } {
			# set cursor to last item in db
			set ret [$dbc get -last]
			error_check_good \
			    verify_key [lindex [lindex $ret 0] 0] $key

			# for error reporting
			set currdatum [lindex [lindex $ret 0] 1]

			# partial-overwrite it
			# (overwrite offsets 1-4 with "bcde"--which they
			# already are)

			# Even though we expect success, we catch this
			# since it might return EINVAL, and we want that
			# to FAIL.
			set errorCode NONE
			set ret [catch {eval $dbc put -current \
				{-partial [list 1 4]} "bcde"} \
				res]
			error_check_good \
				partial_put_valid($currdatum) $errorCode NONE
			error_check_good partial_put($currdatum) $res 0

			# delete it
			error_check_good dbc_del [$dbc del] 0

			#puts $currdatum

			incr count
		}

		error_check_good dbc_close [$dbc close] 0
		error_check_good db_close [$db close] 0
	}
}
