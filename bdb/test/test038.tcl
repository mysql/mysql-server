# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test038.tcl,v 11.12 2000/08/25 14:21:56 sue Exp $
#
# DB Test 38 {access method}
# Use the first 10,000 entries from the dictionary.
# Insert each with self as key and "ndups" duplicates
# For the data field, prepend the letters of the alphabet
# in a random order so that we force the duplicate sorting
# code to do something.
# By setting ndups large, we can make this an off-page test
# After all are entered; test the DB_GET_BOTH functionality
# first by retrieving each dup in the file explicitly.  Then
# remove each duplicate and try DB_GET_BOTH again.
proc test038 { method {nentries 10000} {ndups 5} {tnum 38} args } {
	global alphabet
	global rand_init
	source ./include.tcl

	berkdb srand $rand_init

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	# Create the database and open the dictionary
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test0$tnum.db
		set checkdb $testdir/checkdb.db
		set env NULL
	} else {
		set testfile test0$tnum.db
		set checkdb checkdb.db
		incr eindex
		set env [lindex $args $eindex]
	}
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir $env

	puts "Test0$tnum: \
	    $method ($args) $nentries small sorted dup key/data pairs"
	if { [is_record_based $method] == 1 || \
	    [is_rbtree $method] == 1 } {
		puts "Test0$tnum skipping for method $method"
		return
	}
	set db [eval {berkdb_open -create -truncate -mode 0644 \
		$omethod -dup -dupsort} $args {$testfile}]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]

	set check_db [berkdb_open \
	     -create -truncate -mode 0644 -hash $checkdb]
	error_check_good dbopen:check_db [is_valid_db $check_db] TRUE

	set pflags ""
	set gflags ""
	set txn ""
	set count 0

	# Here is the loop where we put and get each key/data pair
	puts "\tTest0$tnum.a: Put/get loop"
	set dbc [eval {$db cursor} $txn]
	error_check_good cursor_open [is_substr $dbc $db] 1
	while { [gets $did str] != -1 && $count < $nentries } {
		set dups ""
		for { set i 1 } { $i <= $ndups } { incr i } {
			set pref \
			    [string index $alphabet [berkdb random_int 0 25]]
			set pref $pref[string \
			    index $alphabet [berkdb random_int 0 25]]
			while { [string first $pref $dups] != -1 } {
				set pref [string toupper $pref]
				if { [string first $pref $dups] != -1 } {
					set pref [string index $alphabet \
					    [berkdb random_int 0 25]]
					set pref $pref[string index $alphabet \
					    [berkdb random_int 0 25]]
				}
			}
			if { [string length $dups] == 0 } {
				set dups $pref
			} else {
				set dups "$dups $pref"
			}
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
				error_check_good sorted_dups($lastdup,$datastr)\
				    0 1
			}
			incr x
			set lastdup $datastr
		}
		error_check_good "Test0$tnum:ndups:$str" $x $ndups
		incr count
	}
	error_check_good cursor_close [$dbc close] 0
	close $did

	# Now check the duplicates, then delete then recheck
	puts "\tTest0$tnum.b: Checking and Deleting duplicates"
	set dbc [eval {$db cursor} $txn]
	error_check_good cursor_open [is_substr $dbc $db] 1
	set check_c [eval {$check_db cursor} $txn]
	error_check_good cursor_open [is_substr $check_c $check_db] 1

	for {set ndx 0} {$ndx < $ndups} {incr ndx} {
		for {set ret [$check_c get -first]} \
		    {[llength $ret] != 0} \
		    {set ret [$check_c get -next] } {
			set k [lindex [lindex $ret 0] 0]
			set d [lindex [lindex $ret 0] 1]
			error_check_bad data_check:$d [string length $d] 0

			set nn [expr $ndx * 3]
			set pref [string range $d $nn [expr $nn + 1]]
			set data $pref:$k
			set ret [eval {$dbc get} $txn {-get_both $k $data}]
			error_check_good \
			    get_both_key:$k [lindex [lindex $ret 0] 0] $k
			error_check_good \
			    get_both_data:$k [lindex [lindex $ret 0] 1] $data
			set ret [$dbc del]
			error_check_good del $ret 0
			set ret [eval {$db get} $txn {-get_both $k $data}]
			error_check_good error_case:$k [llength $ret] 0

			if {$ndx != 0} {
				set n [expr ($ndx - 1) * 3]
				set pref [string range $d $n [expr $n + 1]]
				set data $pref:$k
				set ret \
				    [eval {$db get} $txn {-get_both $k $data}]
				error_check_good error_case:$k [llength $ret] 0
			}
		}
	}

	error_check_good check_c:close [$check_c close] 0
	error_check_good check_db:close [$check_db close] 0

	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0
}
