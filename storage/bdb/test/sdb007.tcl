# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: sdb007.tcl,v 11.25 2004/09/22 18:01:06 bostic Exp $
#
# TEST	sdb007
# TEST	Tests page size difference errors between subdbs.
# TEST  Test 3 different scenarios for page sizes.
# TEST 	1.  Create/open with a default page size, 2nd subdb create with
# TEST      specified different one, should error.
# TEST  2.  Create/open with specific page size, 2nd subdb create with
# TEST      different one, should error.
# TEST  3.  Create/open with specified page size, 2nd subdb create with
# TEST      same specified size, should succeed.
# TEST  (4th combo of using all defaults is a basic test, done elsewhere)
proc sdb007 { method args } {
	source ./include.tcl
	global is_envmethod

	set db2args [convert_args -btree $args]
	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_queue $method] == 1 } {
		puts "Subdb007: skipping for method $method"
		return
	}
	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Subdb007: skipping for specific page sizes"
		return
	}

	puts "Subdb007: $method ($args) subdb tests with different page sizes"

	set txnenv 0
	set envargs ""
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/subdb007.db
		set env NULL
	} else {
		set testfile subdb007.db
		incr eindex
		set env [lindex $args $eindex]
		set envargs " -env $env "
		set txnenv [is_txnenv $env]
		if { $txnenv == 1 } {
			append args " -auto_commit "
			append envargs " -auto_commit "
			append db2args " -auto_commit "
		}
		set testdir [get_home $env]
	}
	set sub1 "sub1"
	set sub2 "sub2"
	cleanup $testdir $env
	set txn ""

	puts "\tSubdb007.a.0: create subdb with default page size"
	set db [eval {berkdb_open -create -mode 0644} \
	    $args {$omethod $testfile $sub1}]
	error_check_good subdb [is_valid_db $db] TRUE
	#
	# Figure out what the default page size is so that we can
	# guarantee we create it with a different value.
	set statret [$db stat]
	set pgsz 0
	foreach pair $statret {
		set fld [lindex $pair 0]
		if { [string compare $fld {Page size}] == 0 } {
			set pgsz [lindex $pair 1]
		}
	}
	error_check_good dbclose [$db close] 0

	if { $pgsz == 512 } {
		set pgsz2 2048
	} else {
		set pgsz2 512
	}

	puts "\tSubdb007.a.1: create 2nd subdb with specified page size"
	set stat [catch {eval {berkdb_open_noerr -create -btree} \
	    $db2args {-pagesize $pgsz2 $testfile $sub2}} ret]
	error_check_good subdb:pgsz $stat 1
	# We'll get a different error if running in an env,
	# because the env won't have been opened with noerr.
	# Skip the test for what the error is, just getting the
	# error is enough.
	if { $is_envmethod == 0 } {
		error_check_good subdb:fail [is_substr $ret \
		    "Different pagesize specified"] 1
	}

	set ret [eval {berkdb dbremove} $envargs {$testfile}]

	puts "\tSubdb007.b.0: create subdb with specified page size"
	set db [eval {berkdb_open -create -mode 0644} \
	    $args {-pagesize $pgsz2 $omethod $testfile $sub1}]
	error_check_good subdb [is_valid_db $db] TRUE
	set statret [$db stat]
	set newpgsz 0
	foreach pair $statret {
		set fld [lindex $pair 0]
		if { [string compare $fld {Page size}] == 0 } {
			set newpgsz [lindex $pair 1]
		}
	}
	error_check_good pgsize $pgsz2 $newpgsz
	error_check_good dbclose [$db close] 0

	puts "\tSubdb007.b.1: create 2nd subdb with different page size"
	set stat [catch {eval {berkdb_open_noerr -create -btree} \
	    $db2args {-pagesize $pgsz $testfile $sub2}} ret]
	error_check_good subdb:pgsz $stat 1
	if { $is_envmethod == 0 } {
		error_check_good subdb:fail [is_substr $ret \
		    "Different pagesize specified"] 1
	}

	set ret [eval {berkdb dbremove} $envargs {$testfile}]

	puts "\tSubdb007.c.0: create subdb with specified page size"
	set db [eval {berkdb_open -create -mode 0644} \
	    $args {-pagesize $pgsz2 $omethod $testfile $sub1}]
	error_check_good subdb [is_valid_db $db] TRUE
	error_check_good dbclose [$db close] 0

	puts "\tSubdb007.c.1: create 2nd subdb with same specified page size"
	set db [eval {berkdb_open -create -mode 0644} \
	    $args {-pagesize $pgsz2 $omethod $testfile $sub2}]
	error_check_good subdb [is_valid_db $db] TRUE
	error_check_good dbclose [$db close] 0

}
