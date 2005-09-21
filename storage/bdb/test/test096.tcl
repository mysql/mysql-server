# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: test096.tcl,v 11.26 2004/06/10 17:21:20 carol Exp $
#
# TEST	test096
# TEST	Db->truncate test.
# TEST	For all methods:
# TEST		Test that truncate empties an existing database.
# TEST		Test that truncate-write in an aborted txn doesn't
# TEST 		  change the original contents.
# TEST		Test that truncate-write in a committed txn does
# TEST		  overwrite the original contents.
# TEST	For btree and hash, do the same in a database with offpage dups.
proc test096 { method {pagesize 512} {nentries 1000} {ndups 19} args} {
	global fixed_len
	global alphabet
	source ./include.tcl

	set orig_fixed_len $fixed_len
	set args [convert_args $method $args]
	set encargs ""
	set args [split_encargs $args encargs]
	set omethod [convert_method $method]

	puts "Test096: $method db truncate method test"
	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Test096: Skipping for specific pagesizes"
		return
	}

	# Create the database and open the dictionary
	set eindex [lsearch -exact $args "-env"]
	set testfile test096.db
	if { $eindex != -1 } {
		incr eindex
		set env [lindex $args $eindex]
		set txnenv [is_txnenv $env]
		if { $txnenv == 0 } {
			puts "Environment w/o txns specified;  skipping."
			return
		}
		if { $nentries == 1000 } {
			set nentries 100
		}
		reduce_dups nentries ndups
		set testdir [get_home $env]
		set closeenv 0
	} else {
		env_cleanup $testdir

		# We need an env for exclusive-use testing.  Since we are
		# using txns, we need at least 1 lock per record for queue.
		set lockmax [expr $nentries * 2]
		set env [eval {berkdb_env -create -home $testdir \
		    -lock_max $lockmax -txn} $encargs]
		error_check_good env_create [is_valid_env $env] TRUE
		set closeenv 1
	}

	set t1 $testdir/t1

	puts "\tTest096.a: Create database with $nentries entries"
	set db [eval {berkdb_open -create -auto_commit \
	    -env $env $omethod -mode 0644} $args $testfile]
	error_check_good db_open [is_valid_db $db] TRUE
	t96_populate $db $omethod $env $nentries
	error_check_good dbclose [$db close] 0

	puts "\tTest096.b: Truncate database"
	set dbtr [eval {berkdb_open -create -auto_commit \
	    -env $env $omethod -mode 0644} $args $testfile]
	error_check_good db_open [is_valid_db $dbtr] TRUE

	set ret [$dbtr truncate -auto_commit]
	error_check_good dbtrunc $ret $nentries
	error_check_good db_close [$dbtr close] 0

	set db [eval {berkdb_open -env $env} $args $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE
	set number [number_of_entries $db $method]
	error_check_good number_of_entries $number 0
	error_check_good dbclose [$db close] 0
	error_check_good dbverify [verify_dir $testdir "\tTest096.c: "] 0

	# Remove and recreate database.
	puts "\tTest096.d: Recreate database with $nentries entries"
	set db [eval {berkdb_open -create -auto_commit \
	    -env $env $omethod -mode 0644} $args $testfile]
	error_check_good db_open [is_valid_db $db] TRUE
	t96_populate $db $omethod $env $nentries
	error_check_good dbclose [$db close] 0

	puts "\tTest096.e: Truncate and write in a txn, then abort"
	txn_truncate $env $omethod $testfile $nentries abort 1

	set db [eval {berkdb_open -env $env} $args $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	# Database should have original contents since both the truncate
	# and the write were aborted
	set number [number_of_entries $db $method]
	error_check_good number_of_entries $number $nentries
	error_check_good dbclose [$db close] 0

	error_check_good dbverify [verify_dir $testdir "\tTest096.f: "] 0

	puts "\tTest096.g: Truncate and write in a txn, then commit"
	txn_truncate $env $omethod $testfile $nentries commit 1

	set db [eval {berkdb_open -env $env} $args $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	# Database should contain only the new items
	set number [number_of_entries $db $method]
	error_check_good number_of_entries $number [expr $nentries / 2]
	error_check_good dbclose [$db close] 0
	error_check_good dbverify [verify_dir $testdir "\tTest096.h: "] 0

	puts "\tTest096.i: Check proper handling of overflow pages."
	# Large keys and data compared to page size guarantee
	# overflow pages.
	if { [is_fixed_length $method] == 1 } {
		puts "Skipping overflow test for fixed-length method."
	} else {
		set overflowfile overflow096.db
		set data [repeat $alphabet 600]
		set db [eval {berkdb_open -create -auto_commit -pagesize 512 \
		    -env $env $omethod -mode 0644} $args $overflowfile]
		error_check_good db_open [is_valid_db $db] TRUE

		set noverflows 100
		for { set i 1 } { $i <= $noverflows } { incr i } {
			set ret [eval {$db put} -auto_commit \
			    $i [chop_data $method "$i$data"]]
		}

		set stat [$db stat]
		error_check_bad stat:overflow [is_substr $stat \
		    "{{Overflow pages} 0}"] 1

		error_check_good overflow_truncate [$db truncate] $noverflows
		error_check_good overflow_close [$db close] 0
	}

	# Remove database and create a new one with dups.  Skip
	# the rest of the test for methods not supporting dups.
	if { [is_record_based $method] == 1 || \
	    [is_rbtree $method] == 1 } {
		puts "Skipping remainder of test096 for method $method"
		if { $closeenv == 1 } {
			error_check_good envclose [$env close] 0
		}
		return
	}
	set ret [berkdb dbremove -env $env -auto_commit $testfile]
	set ret [berkdb dbremove -env $env -auto_commit $overflowfile]

	puts "\tTest096.j: Create $nentries entries with $ndups duplicates"
	set db [eval {berkdb_open -pagesize $pagesize -dup -auto_commit \
	    -create -env $env $omethod -mode 0644} $args $testfile]
	error_check_good db_open [is_valid_db $db] TRUE

	t96_populate $db $omethod $env $nentries $ndups

	set dlist ""
	for { set i 1 } {$i <= $ndups} {incr i} {
		lappend dlist $i
	}
	set t [$env txn]
	error_check_good txn [is_valid_txn $t $env] TRUE
	set txn "-txn $t"
	dup_check $db $txn $t1 $dlist
	error_check_good txn [$t commit] 0
	puts "\tTest096.k: Verify off page duplicates status"
	set stat [$db stat]
	error_check_bad stat:offpage [is_substr $stat \
	    "{{Duplicate pages} 0}"] 1

	set recs [expr $ndups * $nentries]
	error_check_good dbclose [$db close] 0

	puts "\tTest096.l: Truncate database in a txn then abort"
	txn_truncate $env $omethod $testfile $recs abort

	set db [eval {berkdb_open -auto_commit -env $env} $args $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	set number [number_of_entries $db $method]
	error_check_good number_of_entries $number $recs
	error_check_good dbclose [$db close] 0

	puts "\tTest096.m: Truncate database in a txn then commit"
	txn_truncate $env $omethod $testfile $recs commit

	set db [berkdb_open -auto_commit -env $env $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE
	set number [number_of_entries $db $method]
	error_check_good number_of_entries $number 0
	error_check_good dbclose [$db close] 0

	set testdir [get_home $env]
	error_check_good dbverify [verify_dir $testdir "\tTest096.n: "] 0

	# Remove database, and create a new one with dups.  Test
	# truncate + write within a transaction.
	puts "\tTest096.o: Create $nentries entries with $ndups duplicates"
	set ret [berkdb dbremove -env $env -auto_commit $testfile]
	set db [eval {berkdb_open -pagesize $pagesize -dup -auto_commit \
	    -create -env $env $omethod -mode 0644} $args $testfile]
	error_check_good db_open [is_valid_db $db] TRUE

	t96_populate $db $omethod $env $nentries $ndups

	set dlist ""
	for { set i 1 } {$i <= $ndups} {incr i} {
		lappend dlist $i
	}
	set t [$env txn]
	error_check_good txn [is_valid_txn $t $env] TRUE
	set txn "-txn $t"
	dup_check $db $txn $t1 $dlist
	error_check_good txn [$t commit] 0
	puts "\tTest096.p: Verify off page duplicates status"
	set stat [$db stat]
	error_check_bad stat:offpage [is_substr $stat \
	    "{{Duplicate pages} 0}"] 1

	set recs [expr $ndups * $nentries]
	error_check_good dbclose [$db close] 0

	puts "\tTest096.q: Truncate and write in a txn, then abort"
	txn_truncate $env $omethod $testfile $recs abort 1

	set db [eval {berkdb_open -auto_commit -env $env} $args $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE
	set number [number_of_entries $db $method]
	error_check_good number_of_entries $number $recs
	error_check_good dbclose [$db close] 0

	puts "\tTest096.r: Truncate and write in a txn, then commit"
	txn_truncate $env $omethod $testfile $recs commit 1

	set db [berkdb_open -auto_commit -env $env $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE
	set number [number_of_entries $db $method]
	error_check_good number_of_entries $number [expr $recs / 2]
	error_check_good dbclose [$db close] 0

	puts "\tTest096.s: Check overflow pages with dups."
	set ndups 3
	set db [eval {berkdb_open -create -auto_commit -pagesize 512 \
	    -env $env $omethod -dup -mode 0644} $args $overflowfile]
	error_check_good db_open [is_valid_db $db] TRUE

	for { set i 1 } { $i <= $noverflows } { incr i } {
		for { set j 0 } { $j < $ndups } { incr j } {
			set ret [eval {$db put} -auto_commit \
			    $i [chop_data $method "$i.$j$data"]]
		}
	}

	set stat [$db stat]
	error_check_bad stat:overflow [is_substr $stat \
	    "{{Overflow pages} 0}"] 1

	set nentries [expr $noverflows * $ndups]
	error_check_good overflow_truncate [$db truncate] $nentries
	error_check_good overflow_close [$db close] 0

	set testdir [get_home $env]
	error_check_good dbverify [verify_dir $testdir "\tTest096.t: "] 0

	if { $closeenv == 1 } {
		error_check_good envclose [$env close] 0
	}
}

proc t96_populate {db method env nentries {ndups 1}} {
	global datastr
	global pad_datastr
	source ./include.tcl

	set did [open $dict]
	set count 0
	set txn ""
	set pflags ""
	set gflags ""

	if { [is_record_based $method] == 1 } {
		append gflags "-recno"
	}
	set pad_datastr [pad_data $method $datastr]
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [is_record_based $method] == 1 } {
			set key [expr $count + 1]
		} else {
			set key $str
		}
		if { $ndups > 1 } {
			for { set i 1 } { $i <= $ndups } { incr i } {
				set datastr $i:$str
				set t [$env txn]
				error_check_good txn [is_valid_txn $t $env] TRUE
				set txn "-txn $t"
				set ret [eval {$db put} $txn $pflags \
				    {$key [chop_data $method $datastr]}]
				error_check_good put $ret 0
				error_check_good txn [$t commit] 0
			}
		} else {
			set datastr [reverse $str]
			set t [$env txn]
			error_check_good txn [is_valid_txn $t $env] TRUE
			set txn "-txn $t"
			set ret [eval {$db put} \
			    $txn $pflags {$key [chop_data $method $datastr]}]
			error_check_good put $ret 0
			error_check_good txn [$t commit] 0
		}
		set ret [eval {$db get} $gflags {$key}]
		error_check_good $key:dbget [llength $ret] $ndups
		incr count
	}
	close $did
}

proc number_of_entries { db method } {
	if { [is_record_based $method] == 1 } {
		set dbc [$db cursor]
		set last [$dbc get -last]
		if {[llength $last] == 0} {
			set number 0
		} else {
			set number [lindex [lindex $last 0] 0]
		}
	} else {
		set ret [$db get -glob *]
		set number [llength $ret]
	}
	return $number
}

# Open database.  Truncate in a transaction, optionally with a write
# included in the transaction as well, then abort or commit.  Close database.

proc txn_truncate { env method testfile nentries op {write 0}} {
	set db [eval {berkdb_open -create -auto_commit \
	    -env $env $method -mode 0644} $testfile]
	error_check_good db_open [is_valid_db $db] TRUE

	set txn [$env txn]
	error_check_good txnbegin [is_valid_txn $txn $env] TRUE

	set ret [$db truncate -txn $txn]
	error_check_good dbtrunc $ret $nentries
	if { $write == 1 } {
		for {set i 1} {$i <= [expr $nentries / 2]} {incr i} {
			set ret [eval {$db put} -txn $txn \
			    {$i [chop_data $method "aaaaaaaaaa"]}]
			error_check_good write $ret 0
		}
	}

	error_check_good txn$op [$txn $op] 0
	error_check_good db_close [$db close] 0
}

