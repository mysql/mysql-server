# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: env005.tcl,v 11.8 2000/08/25 14:21:50 sue Exp $
#
# Env Test 5
# Test that using subsystems without initializing them correctly
# returns an error.  Cannot test mpool, because it is assumed
# in the Tcl code.
proc env005 { } {
	source ./include.tcl

	puts "Env005: Uninitialized env subsystems test."

	env_cleanup $testdir
	puts "\tEnv005.a: Creating env with no subsystems."

	set e [berkdb env -create -home $testdir]
	error_check_good dbenv [is_valid_env $e] TRUE
	set db [berkdb_open -create -btree $testdir/env005.db]
	error_check_good dbopen [is_valid_db $db] TRUE

	set rlist {
	{ "lock_detect"			"Env005.b0"}
	{ "lock_get read 1 1"		"Env005.b1"}
	{ "lock_id"			"Env005.b2"}
	{ "lock_stat"			"Env005.b3"}
	{ "log_archive"			"Env005.c0"}
	{ "log_file {1 1}"		"Env005.c1"}
	{ "log_flush"			"Env005.c2"}
	{ "log_get -first"		"Env005.c3"}
	{ "log_put record"		"Env005.c4"}
	{ "log_register $db xxx"	"Env005.c5"}
	{ "log_stat"			"Env005.c6"}
	{ "log_unregister $db"		"Env005.c7"}
	{ "txn"				"Env005.d0"}
	{ "txn_checkpoint"		"Env005.d1"}
	{ "txn_stat"			"Env005.d2"}
	}

	foreach pair $rlist {
		set cmd [lindex $pair 0]
		set msg [lindex $pair 1]
		puts "\t$msg: $cmd"
		set stat [catch {eval $e $cmd} ret]
		error_check_good $cmd $stat 1
		error_check_good $cmd.err [is_substr $ret invalid] 1
	}
	error_check_good dbclose [$db close] 0
	error_check_good envclose [$e close] 0
}
