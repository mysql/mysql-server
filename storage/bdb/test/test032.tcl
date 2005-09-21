# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: test032.tcl,v 11.26 2004/01/28 03:36:31 bostic Exp $
#
# TEST	test032
# TEST	DB_GET_BOTH, DB_GET_BOTH_RANGE
# TEST
# TEST	Use the first 10,000 entries from the dictionary.  Insert each with
# TEST	self as key and "ndups" duplicates.   For the data field, prepend the
# TEST	letters of the alphabet in a random order so we force the duplicate
# TEST  sorting code to do something.  By setting ndups large, we can make
# TEST	this an off-page test.
# TEST
# TEST	Test the DB_GET_BOTH functionality by retrieving each dup in the file
# TEST	explicitly.  Test the DB_GET_BOTH_RANGE functionality by retrieving
# TEST	the unique key prefix (cursor only).  Finally test the failure case.
proc test032 { method {nentries 10000} {ndups 5} {tnum "032"} args } {
	global alphabet rand_init
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	berkdb srand $rand_init

	# Create the database and open the dictionary
	set txnenv 0
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test$tnum.db
		set checkdb $testdir/checkdb.db
		set env NULL
	} else {
		set testfile test$tnum.db
		set checkdb checkdb.db
		incr eindex
		set env [lindex $args $eindex]
		set txnenv [is_txnenv $env]
		if { $txnenv == 1 } {
			append args " -auto_commit "
			#
			# If we are using txns and running with the
			# default, set the default down a bit.
			#
			if { $nentries == 10000 } {
				set nentries 100
			}
			reduce_dups nentries ndups
		}
		set testdir [get_home $env]
	}
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir $env

	puts "Test$tnum:\
	    $method ($args) $nentries small sorted $ndups dup key/data pairs"
	if { [is_record_based $method] == 1 || \
	    [is_rbtree $method] == 1 } {
		puts "Test$tnum skipping for method $omethod"
		return
	}
	set db [eval {berkdb_open -create -mode 0644 \
	    $omethod -dup -dupsort} $args {$testfile} ]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]

	set check_db [eval {berkdb_open \
	     -create -mode 0644} $args {-hash $checkdb}]
	error_check_good dbopen:check_db [is_valid_db $check_db] TRUE

	set pflags ""
	set gflags ""
	set txn ""
	set count 0

	# Here is the loop where we put and get each key/data pair
	puts "\tTest$tnum.a: Put/get loop"
	if { $txnenv == 1 } {
		set t [$env txn]
		error_check_good txn [is_valid_txn $t $env] TRUE
		set txn "-txn $t"
	}
	set dbc [eval {$db cursor} $txn]
	error_check_good cursor_open [is_valid_cursor $dbc $db] TRUE
	while { [gets $did str] != -1 && $count < $nentries } {
		# Re-initialize random string generator
		randstring_init $ndups

		set dups ""
		for { set i 1 } { $i <= $ndups } { incr i } {
			set pref [randstring]
			set dups $dups$pref
			set datastr $pref:$str
			set ret [eval {$db put} \
			    $txn $pflags {$str [chop_data $method $datastr]}]
			error_check_good put $ret 0
		}
		set ret [eval {$check_db put} \
		    $txn $pflags {$str [chop_data $method $dups]}]
		error_check_good checkdb_put $ret 0

		# Now retrieve all the keys matching this key
		set x 0
		set lastdup ""
		for {set ret [$dbc get -set $str]} \
		    {[llength $ret] != 0} \
		    {set ret [$dbc get -nextdup] } {
			set k [lindex [lindex $ret 0] 0]
			if { [string compare $k $str] != 0 } {
				break
			}
			set datastr [lindex [lindex $ret 0] 1]
			if {[string length $datastr] == 0} {
				break
			}
			if {[string compare $lastdup $datastr] > 0} {
				error_check_good \
				    sorted_dups($lastdup,$datastr) 0 1
			}
			incr x
			set lastdup $datastr
		}

		error_check_good "Test$tnum:ndups:$str" $x $ndups
		incr count
	}
	error_check_good cursor_close [$dbc close] 0
	if { $txnenv == 1 } {
		error_check_good txn [$t commit] 0
	}
	close $did

	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest$tnum.b: Checking file for correct duplicates (no cursor)"
	if { $txnenv == 1 } {
		set t [$env txn]
		error_check_good txn [is_valid_txn $t $env] TRUE
		set txn "-txn $t"
	}
	set check_c [eval {$check_db cursor} $txn]
	error_check_good check_c_open(2) \
	    [is_valid_cursor $check_c $check_db] TRUE

	for {set ndx 0} {$ndx < [expr 4 * $ndups]} {incr ndx 4} {
		for {set ret [$check_c get -first]} \
		    {[llength $ret] != 0} \
		    {set ret [$check_c get -next] } {
			set k [lindex [lindex $ret 0] 0]
			set d [lindex [lindex $ret 0] 1]
			error_check_bad data_check:$d [string length $d] 0

			set pref [string range $d $ndx [expr $ndx + 3]]
			set data $pref:$k
			set ret [eval {$db get} $txn {-get_both $k $data}]
			error_check_good \
			    get_both_data:$k $ret [list [list $k $data]]
		}
	}

	$db sync

	# Now repeat the above test using cursor ops
	puts "\tTest$tnum.c: Checking file for correct duplicates (cursor)"
	set dbc [eval {$db cursor} $txn]
	error_check_good cursor_open [is_valid_cursor $dbc $db] TRUE

	for {set ndx 0} {$ndx < [expr 4 * $ndups]} {incr ndx 4} {
		for {set ret [$check_c get -first]} \
		    {[llength $ret] != 0} \
		    {set ret [$check_c get -next] } {
			set k [lindex [lindex $ret 0] 0]
			set d [lindex [lindex $ret 0] 1]
			error_check_bad data_check:$d [string length $d] 0

			set pref [string range $d $ndx [expr $ndx + 3]]
			set data $pref:$k
			set ret [eval {$dbc get} {-get_both $k $data}]
			error_check_good \
			    curs_get_both_data:$k $ret [list [list $k $data]]

			set ret [eval {$dbc get} {-get_both_range $k $pref}]
			error_check_good \
			    curs_get_both_range:$k $ret [list [list $k $data]]
		}
	}

	# Now check the error case
	puts "\tTest$tnum.d: Check error case (no cursor)"
	for {set ret [$check_c get -first]} \
	    {[llength $ret] != 0} \
	    {set ret [$check_c get -next] } {
		set k [lindex [lindex $ret 0] 0]
		set d [lindex [lindex $ret 0] 1]
		error_check_bad data_check:$d [string length $d] 0

		set data XXX$k
		set ret [eval {$db get} $txn {-get_both $k $data}]
		error_check_good error_case:$k [llength $ret] 0
	}

	# Now check the error case
	puts "\tTest$tnum.e: Check error case (cursor)"
	for {set ret [$check_c get -first]} \
	    {[llength $ret] != 0} \
	    {set ret [$check_c get -next] } {
		set k [lindex [lindex $ret 0] 0]
		set d [lindex [lindex $ret 0] 1]
		error_check_bad data_check:$d [string length $d] 0

		set data XXX$k
		set ret [eval {$dbc get} {-get_both $k $data}]
		error_check_good error_case:$k [llength $ret] 0
	}

	error_check_good check_c:close [$check_c close] 0
	error_check_good dbc_close [$dbc close] 0
	if { $txnenv == 1 } {
		error_check_good txn [$t commit] 0
	}
	error_check_good check_db:close [$check_db close] 0
	error_check_good db_close [$db close] 0
}
