# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: sdb008.tcl,v 11.25 2002/07/11 18:53:46 sandstro Exp $
# TEST	subdb008
# TEST	Tests lorder difference errors between subdbs.
# TEST  Test 3 different scenarios for lorder.
# TEST  1.  Create/open with specific lorder, 2nd subdb create with
# TEST      different one, should error.
# TEST 	2.  Create/open with a default lorder 2nd subdb create with
# TEST      specified different one, should error.
# TEST  3.  Create/open with specified lorder, 2nd subdb create with
# TEST      same specified lorder, should succeed.
# TEST  (4th combo of using all defaults is a basic test, done elsewhere)
proc subdb008 { method args } {
	source ./include.tcl

	set db2args [convert_args -btree $args]
	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_queue $method] == 1 } {
		puts "Subdb008: skipping for method $method"
		return
	}
	set txnenv 0
	set envargs ""
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/subdb008.db
		set env NULL
	} else {
		set testfile subdb008.db
		incr eindex
		set env [lindex $args $eindex]
		set envargs "-env $env"
		set txnenv [is_txnenv $env]
		if { $txnenv == 1 } {
			append args " -auto_commit "
			append db2args " -auto_commit "
			append envargs " -auto_commit "
		}
		set testdir [get_home $env]
	}
	puts "Subdb008: $method ($args) subdb tests with different lorders"

	set sub1 "sub1"
	set sub2 "sub2"
	cleanup $testdir $env

	puts "\tSubdb008.b.0: create subdb with specified lorder"
	set db [eval {berkdb_open -create -mode 0644} \
	    $args {-lorder 4321 $omethod $testfile $sub1}]
	error_check_good subdb [is_valid_db $db] TRUE
	# Figure out what the default lorder is so that we can
	# guarantee we create it with a different value later.
	set is_swap [$db is_byteswapped]
	if { $is_swap } {
		set other 4321
	} else {
 		set other 1234
	}
	error_check_good dbclose [$db close] 0

	puts "\tSubdb008.b.1: create 2nd subdb with different lorder"
	set stat [catch {eval {berkdb_open_noerr -create $omethod} \
	    $args {-lorder 1234 $testfile $sub2}} ret]
	error_check_good subdb:lorder $stat 1
	error_check_good subdb:fail [is_substr $ret \
	    "Different lorder specified"] 1

	set ret [eval {berkdb dbremove} $envargs {$testfile}]

	puts "\tSubdb008.c.0: create subdb with opposite specified lorder"
	set db [eval {berkdb_open -create -mode 0644} \
	    $args {-lorder 1234 $omethod $testfile $sub1}]
	error_check_good subdb [is_valid_db $db] TRUE
	error_check_good dbclose [$db close] 0

	puts "\tSubdb008.c.1: create 2nd subdb with different lorder"
	set stat [catch {eval {berkdb_open_noerr -create $omethod} \
	    $args {-lorder 4321 $testfile $sub2}} ret]
	error_check_good subdb:lorder $stat 1
	error_check_good subdb:fail [is_substr $ret \
	    "Different lorder specified"] 1

	set ret [eval {berkdb dbremove} $envargs {$testfile}]

	puts "\tSubdb008.d.0: create subdb with default lorder"
	set db [eval {berkdb_open -create -mode 0644} \
	    $args {$omethod $testfile $sub1}]
	error_check_good subdb [is_valid_db $db] TRUE
	error_check_good dbclose [$db close] 0

	puts "\tSubdb008.d.1: create 2nd subdb with different lorder"
	set stat [catch {eval {berkdb_open_noerr -create -btree} \
	    $db2args {-lorder $other $testfile $sub2}} ret]
	error_check_good subdb:lorder $stat 1
	error_check_good subdb:fail [is_substr $ret \
	    "Different lorder specified"] 1

	set ret [eval {berkdb dbremove} $envargs {$testfile}]

	puts "\tSubdb008.e.0: create subdb with specified lorder"
	set db [eval {berkdb_open -create -mode 0644} \
	    $args {-lorder $other $omethod $testfile $sub1}]
	error_check_good subdb [is_valid_db $db] TRUE
	error_check_good dbclose [$db close] 0

	puts "\tSubdb008.e.1: create 2nd subdb with same specified lorder"
	set db [eval {berkdb_open -create -mode 0644} \
	    $args {-lorder $other $omethod $testfile $sub2}]
	error_check_good subdb [is_valid_db $db] TRUE
	error_check_good dbclose [$db close] 0

}
