# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: rpc002.tcl,v 1.7 2000/10/27 13:23:56 sue Exp $
#
# RPC Test 2
# Test invalid RPC functions and make sure we error them correctly
proc rpc002 { } {
	global __debug_on
	global __debug_print
	global errorInfo
	source ./include.tcl

	set testfile "rpc002.db"
	set home [file tail $rpc_testdir]
	#
	# First start the server.
	#
	puts "Rpc002: Unsupported interface test"
	if { [string compare $rpc_server "localhost"] == 0 } {
	       set dpid [exec $util_path/berkeley_db_svc -h $rpc_testdir &]
	} else {
	       set dpid [exec rsh $rpc_server $rpc_path/berkeley_db_svc \
		   -h $rpc_testdir &]
	}
	puts "\tRpc002.a: Started server, pid $dpid"
	tclsleep 2
	remote_cleanup $rpc_server $rpc_testdir $testdir

	puts "\tRpc002.b: Unsupported env options"
	#
	# Test each "pre-open" option for env's.  These need to be
	# tested on the 'berkdb env' line.
	#
	set rlist {
	{ "-data_dir $rpc_testdir"	"Rpc002.b0"}
	{ "-log_buffer 512"		"Rpc002.b1"}
	{ "-log_dir $rpc_testdir"	"Rpc002.b2"}
	{ "-log_max 100"		"Rpc002.b3"}
	{ "-lock_conflict {3 {0 0 0 0 0 1 0 1 1}}"	"Rpc002.b4"}
	{ "-lock_detect default"	"Rpc002.b5"}
	{ "-lock_max 100"		"Rpc002.b6"}
	{ "-mmapsize 100"		"Rpc002.b7"}
	{ "-shm_key 100"		"Rpc002.b9"}
	{ "-tmp_dir $rpc_testdir"	"Rpc002.b10"}
	{ "-txn_max 100"		"Rpc002.b11"}
	{ "-txn_timestamp 100"		"Rpc002.b12"}
	{ "-verbose {recovery on}"		"Rpc002.b13"}
	}

	set e "berkdb env -create -mode 0644 -home $home -server $rpc_server \
	    -client_timeout 10000 -txn"
	foreach pair $rlist {
		set cmd [lindex $pair 0]
		set msg [lindex $pair 1]
		puts "\t$msg: $cmd"

		set stat [catch {eval $e $cmd} ret]
		error_check_good $cmd $stat 1
		error_check_good $cmd.err \
		    [is_substr $errorInfo "meaningless in RPC env"] 1
	}

	#
	# Open an env with all the subsystems (-txn implies all
	# the rest)
	#
	puts "\tRpc002.c: Unsupported env related interfaces"
	set env [eval {berkdb env -create -mode 0644 -home $home \
	    -server $rpc_server -client_timeout 10000 -txn}]
	error_check_good envopen [is_valid_env $env] TRUE
	set dbcmd "berkdb_open_noerr -create -btree -mode 0644 -env $env \
	    $testfile"
	set db [eval $dbcmd]
	error_check_good dbopen [is_valid_db $db] TRUE

	#
	# Test each "post-open" option relating to envs, txns, locks,
	# logs and mpools.
	#
	set rlist {
	{ " lock_detect default"	"Rpc002.c0"}
	{ " lock_get read 1 $env"	"Rpc002.c1"}
	{ " lock_id"			"Rpc002.c2"}
	{ " lock_stat"			"Rpc002.c3"}
	{ " lock_vec 1 {get $env read}"	"Rpc002.c4"}
	{ " log_archive"		"Rpc002.c5"}
	{ " log_file {0 0}"		"Rpc002.c6"}
	{ " log_flush"			"Rpc002.c7"}
	{ " log_get -current"		"Rpc002.c8"}
	{ " log_register $db $testfile"	"Rpc002.c9"}
	{ " log_stat"			"Rpc002.c10"}
	{ " log_unregister $db"		"Rpc002.c11"}
	{ " mpool -create -pagesize 512"	"Rpc002.c12"}
	{ " mpool_stat"			"Rpc002.c13"}
	{ " mpool_sync {0 0}"		"Rpc002.c14"}
	{ " mpool_trickle 50"		"Rpc002.c15"}
	{ " txn_checkpoint -min 1"	"Rpc002.c16"}
	{ " txn_stat"			"Rpc002.c17"}
	}

	foreach pair $rlist {
		set cmd [lindex $pair 0]
		set msg [lindex $pair 1]
		puts "\t$msg: $cmd"

		set stat [catch {eval $env $cmd} ret]
		error_check_good $cmd $stat 1
		error_check_good $cmd.err \
		    [is_substr $errorInfo "meaningless in RPC env"] 1
	}
	error_check_good dbclose [$db close] 0

	#
	# The database operations that aren't supported are few
	# because mostly they are the ones Tcl doesn't support
	# either so we have no way to get at them.  Test what we can.
	#
	puts "\tRpc002.d: Unsupported database related interfaces"
	#
	# NOTE: the type of database doesn't matter, just use btree.
	#
	puts "\tRpc002.d0: -cachesize"
	set dbcmd "berkdb_open_noerr -create -btree -mode 0644 -env $env \
	    -cachesize {0 65536 0} $testfile"
	set stat [catch {eval $dbcmd} ret]
	error_check_good dbopen_cache $stat 1
	error_check_good dbopen_cache_err \
	    [is_substr $errorInfo "meaningless in RPC env"] 1

	puts "\tRpc002.d1: Try to upgrade a database"
	#
	# NOTE: the type of database doesn't matter, just use btree.
	set stat [catch {eval {berkdb upgrade -env} $env $testfile} ret]
	error_check_good dbupgrade $stat 1
	error_check_good dbupgrade_err \
	    [is_substr $errorInfo "meaningless in RPC env"] 1

	error_check_good envclose [$env close] 0

	exec $KILL $dpid
}
