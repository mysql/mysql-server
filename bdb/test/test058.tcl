# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test058.tcl,v 11.14 2000/08/25 14:21:57 sue Exp $
#
proc test058 { method args } {
	source ./include.tcl

	#
	# If we are using an env, then skip this test.  It needs its own.
	set eindex [lsearch -exact $args "-env"]
	if { $eindex != -1 } {
		incr eindex
		set env [lindex $args $eindex]
		puts "Test058 skipping for env $env"
		return
	}
	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_record_based $method] == 1 || [is_rbtree $method] == 1 } {
		puts "Test058: skipping for method $method"
		return
	}
	puts "Test058: $method delete dups after inserting after duped key."

	# environment
	env_cleanup $testdir
	set eflags "-create -txn -home $testdir"
	set env [eval {berkdb env} $eflags]
	error_check_good env [is_valid_env $env] TRUE

	# db open
	set flags "-create -mode 0644 -dup -env $env $args"
	set db [eval {berkdb_open} $flags $omethod "test058.db"]
	error_check_good dbopen [is_valid_db $db] TRUE

	set tn ""
	set tid ""
	set tn [$env txn]
	set tflags "-txn $tn"

	puts "\tTest058.a: Adding 10 duplicates"
	# Add a bunch of dups
	for { set i 0 } { $i < 10 } {incr i} {
		set ret \
		    [eval {$db put} $tflags {doghouse $i"DUPLICATE_DATA_VALUE"}]
		error_check_good db_put $ret 0
	}

	puts "\tTest058.b: Adding key after duplicates"
	# Now add one more key/data AFTER the dup set.
	set ret [eval {$db put} $tflags {zebrahouse NOT_A_DUP}]
	error_check_good db_put $ret 0

	error_check_good txn_commit [$tn commit] 0

	set tn [$env txn]
	error_check_good txnbegin [is_substr $tn $env] 1
	set tflags "-txn $tn"

	# Now delete everything
	puts "\tTest058.c: Deleting duplicated key"
	set ret [eval {$db del} $tflags {doghouse}]
	error_check_good del $ret 0

	# Now reput everything
	set pad \
	    abcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuvabcdefghijklmnopqrtsuv

	puts "\tTest058.d: Reputting duplicates with big data vals"
	for { set i 0 } { $i < 10 } {incr i} {
		set ret [eval {$db put} \
		    $tflags {doghouse $i"DUPLICATE_DATA_VALUE"$pad}]
		error_check_good db_put $ret 0
	}
	error_check_good txn_commit [$tn commit] 0

	# Check duplicates for order
	set dbc [$db cursor]
	error_check_good db_cursor [is_substr $dbc $db] 1

	puts "\tTest058.e: Verifying that duplicates are in order."
	set i 0
	for { set ret [$dbc get -set doghouse] } \
	    {$i < 10 && [llength $ret] != 0} \
	    { set ret [$dbc get -nextdup] } {
		set data [lindex [lindex $ret 0] 1]
		error_check_good \
		    duplicate_value $data $i"DUPLICATE_DATA_VALUE"$pad
		incr i
	}

	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0
	reset_env $env
}
