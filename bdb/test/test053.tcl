# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test053.tcl,v 11.12 2000/12/11 17:24:55 sue Exp $
#
# Test53: test of the DB_REVSPLITOFF flag in the btree and
# Btree-w-recnum methods
proc test053 { method args } {
	global alphabet
	global errorCode
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	puts "\tTest053: Test of cursor stability across btree splits."
	if { [is_btree $method] != 1 && [is_rbtree $method] != 1 } {
		puts "Test053: skipping for method $method."
		return
	}

	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Test053: skipping for specific pagesizes"
		return
	}

	set txn ""
	set flags ""

	puts "\tTest053.a: Create $omethod $args database."
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test053.db
		set env NULL
	} else {
		set testfile test053.db
		incr eindex
		set env [lindex $args $eindex]
	}
	set t1 $testdir/t1
	cleanup $testdir $env

	set oflags \
	    "-create -truncate -revsplitoff -pagesize 1024 $args $omethod"
	set db [eval {berkdb_open} $oflags $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	set nkeys 8
	set npages 15

	# We want to create a db with npages leaf pages, and have each page
	# be near full with keys that we can predict. We set pagesize above
	# to 1024 bytes, it should breakdown as follows (per page):
	#
	#	~20 bytes overhead
	#	key: ~4 bytes overhead, XXX0N where X is a letter, N is 0-9
	#	data: ~4 bytes overhead, + 100 bytes
	#
	# then, with 8 keys/page we should be just under 1024 bytes
	puts "\tTest053.b: Create $npages pages with $nkeys pairs on each."
	set keystring [string range $alphabet 0 [expr $npages -1]]
	set data [repeat DATA 22]
	for { set i 0 } { $i < $npages } {incr i } {
		set key ""
		set keyroot \
		    [repeat [string toupper [string range $keystring $i $i]] 3]
		set key_set($i) $keyroot
		for {set j 0} { $j < $nkeys} {incr j} {
			if { $j < 10 } {
				set key [set keyroot]0$j
			} else {
				set key $keyroot$j
			}
			set ret [$db put $key $data]
			error_check_good dbput $ret 0
		}
	}

	puts "\tTest053.c: Check page count."
	error_check_good page_count:check \
	    [is_substr [$db stat] "{Leaf pages} $npages"] 1

	puts "\tTest053.d: Delete all but one key per page."
	for {set i 0} { $i < $npages } {incr i } {
		for {set j 1} { $j < $nkeys } {incr j } {
			set ret [$db del $key_set($i)0$j]
			error_check_good dbdel $ret 0
		}
	}
	puts "\tTest053.e: Check to make sure all pages are still there."
	error_check_good page_count:check \
	    [is_substr [$db stat] "{Leaf pages} $npages"] 1

	set dbc [$db cursor]
	error_check_good db:cursor [is_substr $dbc $db] 1

	# walk cursor through tree forward, backward.
	# delete one key, repeat
	for {set i 0} { $i < $npages} {incr i} {
		puts -nonewline \
		    "\tTest053.f.$i: Walk curs through tree: forward..."
		for { set j $i; set curr [$dbc get -first]} { $j < $npages} { \
		    incr j; set curr [$dbc get -next]} {
			error_check_bad dbc:get:next [llength $curr] 0
			error_check_good dbc:get:keys \
			    [lindex [lindex $curr 0] 0] $key_set($j)00
		}
		puts -nonewline "backward..."
		for { set j [expr $npages - 1]; set curr [$dbc get -last]} { \
		    $j >= $i } { \
		    set j [incr j -1]; set curr [$dbc get -prev]} {
			error_check_bad dbc:get:prev [llength $curr] 0
			error_check_good dbc:get:keys \
			    [lindex [lindex $curr 0] 0] $key_set($j)00
		}
		puts "complete."

		if { [is_rbtree $method] == 1} {
			puts "\t\tTest053.f.$i:\
			    Walk through tree with record numbers."
			for {set j 1} {$j <= [expr $npages - $i]} {incr j} {
				set curr [$db get -recno $j]
				error_check_bad \
				    db_get:recno:$j [llength $curr] 0
				error_check_good db_get:recno:keys:$j \
				    [lindex [lindex $curr 0] 0] \
				    $key_set([expr $j + $i - 1])00
			}
		}
		puts "\tTest053.g.$i:\
		    Delete single key ([expr $npages - $i] keys left)."
		set ret [$db del $key_set($i)00]
		error_check_good dbdel $ret 0
		error_check_good del:check \
		    [llength [$db get $key_set($i)00]] 0
	}

	# end for loop, verify db_notfound
	set ret [$dbc get -first]
	error_check_good dbc:get:verify [llength $ret] 0

	# loop: until single key restored on each page
	for {set i 0} { $i < $npages} {incr i} {
		puts "\tTest053.i.$i:\
		    Restore single key ([expr $i + 1] keys in tree)."
		set ret [$db put $key_set($i)00 $data]
		error_check_good dbput $ret 0

		puts -nonewline \
		    "\tTest053.j: Walk cursor through tree: forward..."
		for { set j 0; set curr [$dbc get -first]} { $j <= $i} {\
				  incr j; set curr [$dbc get -next]} {
			error_check_bad dbc:get:next [llength $curr] 0
			error_check_good dbc:get:keys \
			    [lindex [lindex $curr 0] 0] $key_set($j)00
		}
		error_check_good dbc:get:next [llength $curr] 0

		puts -nonewline "backward..."
		for { set j $i; set curr [$dbc get -last]} { \
		    $j >= 0 } { \
		    set j [incr j -1]; set curr [$dbc get -prev]} {
			error_check_bad dbc:get:prev [llength $curr] 0
			error_check_good dbc:get:keys \
			    [lindex [lindex $curr 0] 0] $key_set($j)00
		}
		puts "complete."
		error_check_good dbc:get:prev [llength $curr] 0

		if { [is_rbtree $method] == 1} {
			puts "\t\tTest053.k.$i:\
			    Walk through tree with record numbers."
			for {set j 1} {$j <= [expr $i + 1]} {incr j} {
				set curr [$db get -recno $j]
				error_check_bad \
				    db_get:recno:$j [llength $curr] 0
				error_check_good db_get:recno:keys:$j \
				    [lindex [lindex $curr 0] 0] \
				    $key_set([expr $j - 1])00
			}
		}
	}

	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0

	puts "Test053 complete."
}
