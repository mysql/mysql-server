# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: rpc004.tcl,v 11.6 2002/07/16 20:53:03 bostic Exp $
#
# TEST	rpc004
# TEST	Test RPC server and security
proc rpc004 { } {
	global __debug_on
	global __debug_print
	global errorInfo
	global passwd
	global rpc_svc
	source ./include.tcl

	puts "Rpc004: RPC server + security"
	cleanup $testdir NULL
	if { [string compare $rpc_server "localhost"] == 0 } {
	       set dpid [exec $util_path/$rpc_svc \
		   -h $rpc_testdir -P $passwd &]
	} else {
	       set dpid [exec rsh $rpc_server $rpc_path/$rpc_svc \
		   -h $rpc_testdir -P $passwd &]
	}
	puts "\tRpc004.a: Started server, pid $dpid"

	tclsleep 2
	remote_cleanup $rpc_server $rpc_testdir $testdir
	puts "\tRpc004.b: Creating environment"

	set testfile "rpc004.db"
	set testfile1 "rpc004a.db"
	set home [file tail $rpc_testdir]

	set env [eval {berkdb_env -create -mode 0644 -home $home \
	    -server $rpc_server -encryptaes $passwd -txn}]
	error_check_good lock_env:open [is_valid_env $env] TRUE

	puts "\tRpc004.c: Opening a non-encrypted database"
	#
	# NOTE: the type of database doesn't matter, just use btree.
	set db [eval {berkdb_open -auto_commit -create -btree -mode 0644} \
	    -env $env $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	puts "\tRpc004.d: Opening an encrypted database"
	set db1 [eval {berkdb_open -auto_commit -create -btree -mode 0644} \
	    -env $env -encrypt $testfile1]
	error_check_good dbopen [is_valid_db $db1] TRUE

	set txn [$env txn]
	error_check_good txn [is_valid_txn $txn $env] TRUE
	puts "\tRpc004.e: Put/get on both databases"
	set key "key"
	set data "data"

	set ret [$db put -txn $txn $key $data]
	error_check_good db_put $ret 0
	set ret [$db get -txn $txn $key]
	error_check_good db_get $ret [list [list $key $data]]
	set ret [$db1 put -txn $txn $key $data]
	error_check_good db1_put $ret 0
	set ret [$db1 get -txn $txn $key]
	error_check_good db1_get $ret [list [list $key $data]]

	error_check_good txn_commit [$txn commit] 0
	error_check_good db_close [$db close] 0
	error_check_good db1_close [$db1 close] 0
	error_check_good env_close [$env close] 0

	# Cleanup our environment because it's encrypted
	remote_cleanup $rpc_server $rpc_testdir $testdir
	tclkill $dpid
}
