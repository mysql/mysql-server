# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test078.tcl,v 1.9 2000/12/11 17:24:55 sue Exp $
#
# DB Test 78: Test of DBC->c_count(). [#303]
proc test078 { method { nkeys 100 } { pagesize 512 } { tnum 78 } args } {
	source ./include.tcl
	global alphabet rand_init

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	puts "Test0$tnum: Test of key counts."

	berkdb srand $rand_init

	set eindex [lsearch -exact $args "-env"]
	if { $eindex == -1 } {
		set testfile $testdir/test0$tnum.db
		set env NULL
	} else {
		set testfile test0$tnum.db
		incr eindex
		set env [lindex $args $eindex]
	}
	cleanup $testdir $env

	puts "\tTest0$tnum.a: No duplicates, trivial answer."
	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Test078: skipping for specific pagesizes"
		return
	}

	set db [eval {berkdb_open -create -truncate -mode 0644\
	    -pagesize $pagesize} $omethod $args {$testfile}]
	error_check_good db_open [is_valid_db $db] TRUE

	for { set i 1 } { $i <= $nkeys } { incr i } {
		error_check_good put.a($i) [$db put $i\
		    [pad_data $method $alphabet$i]] 0
		error_check_good count.a [$db count $i] 1
	}
	error_check_good db_close.a [$db close] 0

	if { [is_record_based $method] == 1 || [is_rbtree $method] == 1 } {
		puts \
	    "\tTest0$tnum.b: Duplicates not supported in $method, skipping."
		return
	}

	foreach tuple {{b sorted "-dup -dupsort"} {c unsorted "-dup"}} {
		set letter [lindex $tuple 0]
		set dupopt [lindex $tuple 2]

		puts "\tTest0$tnum.$letter: Duplicates ([lindex $tuple 1])."

		puts "\t\tTest0$tnum.$letter.1: Populating database."

		set db [eval {berkdb_open -create -truncate -mode 0644\
		    -pagesize $pagesize} $dupopt $omethod $args {$testfile}]
		error_check_good db_open [is_valid_db $db] TRUE

		for { set i 1 } { $i <= $nkeys } { incr i } {
			for { set j 0 } { $j < $i } { incr j } {
				error_check_good put.$letter,$i [$db put $i\
				    [pad_data $method $j$alphabet]] 0
			}
		}

		puts -nonewline "\t\tTest0$tnum.$letter.2: "
		puts "Verifying dup counts on first dup."
		for { set i 1 } { $i < $nkeys } { incr i } {
			error_check_good count.$letter,$i \
			    [$db count $i] $i
		}

		puts -nonewline "\t\tTest0$tnum.$letter.3: "
		puts "Verifying dup counts on random dup."
		for { set i 1 } { $i < $nkeys } { incr i } {
			set key [berkdb random_int 1 $nkeys]
			error_check_good count.$letter,$i \
			    [$db count $i] $i
		}
		error_check_good db_close.$letter [$db close] 0
	}
}
