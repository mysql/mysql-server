# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test083.tcl,v 11.6 2000/12/11 17:24:55 sue Exp $
#
# Test 83.
# Test of DB->key_range
proc test083 { method {pgsz 512} {maxitems 5000} {step 2} args} {
	source ./include.tcl
	set omethod [convert_method $method]
	set args [convert_args $method $args]

	puts "Test083 $method ($args): Test of DB->key_range"
	if { [is_btree $method] != 1 } {
		puts "\tTest083: Skipping for method $method."
		return
	}
	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Test083: skipping for specific pagesizes"
		return
	}

	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	set eindex [lsearch -exact $args "-env"]
	if { $eindex == -1 } {
		set testfile $testdir/test083.db
		set env NULL
	} else {
		set testfile test083.db
		incr eindex
		set env [lindex $args $eindex]
	}

	# We assume that numbers will be at most six digits wide
	error_check_bad maxitems_range [expr $maxitems > 999999] 1

	# We want to test key_range on a variety of sizes of btree.
	# Start at ten keys and work up to $maxitems keys, at each step
	# multiplying the number of keys by $step.
	for { set nitems 10 } { $nitems <= $maxitems }\
	    { set nitems [expr $nitems * $step] } {

		puts "\tTest083.a: Opening new database"
		cleanup $testdir $env
		set db [eval {berkdb_open -create -truncate -mode 0644} \
		    -pagesize $pgsz $omethod $args $testfile]
		error_check_good dbopen [is_valid_db $db] TRUE

		t83_build $db $nitems
		t83_test $db $nitems

		error_check_good db_close [$db close] 0
	}
}

proc t83_build { db nitems } {
	source ./include.tcl

	puts "\tTest083.b: Populating database with $nitems keys"

	set keylist {}
	puts "\t\tTest083.b.1: Generating key list"
	for { set i 0 } { $i < $nitems } { incr i } {
		lappend keylist $i
	}

	# With randomly ordered insertions, the range of errors we
	# get from key_range can be unpredictably high [#2134].  For now,
	# just skip the randomization step.
	#puts "\t\tTest083.b.2: Randomizing key list"
	#set keylist [randomize_list $keylist]

	#puts "\t\tTest083.b.3: Populating database with randomized keys"

	puts "\t\tTest083.b.2: Populating database"
	set data [repeat . 50]

	foreach keynum $keylist {
		error_check_good db_put [$db put key[format %6d $keynum] \
		    $data] 0
	}
}

proc t83_test { db nitems } {
	# Look at the first key, then at keys about 1/4, 1/2, 3/4, and
	# all the way through the database.  Make sure the key_ranges
	# aren't off by more than 10%.

	set dbc [$db cursor]
	error_check_good dbc [is_valid_cursor $dbc $db] TRUE

	puts "\tTest083.c: Verifying ranges..."

	for { set i 0 } { $i < $nitems } \
	    { incr i [expr $nitems / [berkdb random_int 3 16]] } {
		puts "\t\t...key $i"
		error_check_bad key0 [llength [set dbt [$dbc get -first]]] 0

		for { set j 0 } { $j < $i } { incr j } {
			error_check_bad key$j \
			    [llength [set dbt [$dbc get -next]]] 0
		}

		set ranges [$db keyrange [lindex [lindex $dbt 0] 0]]

		#puts $ranges
		error_check_good howmanyranges [llength $ranges] 3

		set lessthan [lindex $ranges 0]
		set morethan [lindex $ranges 2]

		set rangesum [expr $lessthan + [lindex $ranges 1] + $morethan]

		roughly_equal $rangesum 1 0.05

		# Wild guess.
		if { $nitems < 500 } {
			set tol 0.3
		} elseif { $nitems > 500 } {
			set tol 0.15
		}

		roughly_equal $lessthan [expr $i * 1.0 / $nitems] $tol

	}

	error_check_good dbc_close [$dbc close] 0
}

proc roughly_equal { a b tol } {
	error_check_good "$a =~ $b" [expr $a - $b < $tol] 1
}
