#
# Copyright (c) 2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep004.tcl,v 1.5 2002/08/08 18:13:12 sue Exp $
#
# TEST rep004
# TEST Test of DB_REP_LOGSONLY.
# TEST
# TEST Run a quick put test in a master environment that has one logs-only
# TEST client.  Shut down, then run catastrophic recovery in the logs-only
# TEST client and check that the database is present and populated.

proc rep004 { method { nitems 10 } { tnum "04" } args } {
	source ./include.tcl
	global testdir

	env_cleanup $testdir
	set dbname rep0$tnum.db

	set omethod [convert_method $method]
	set oargs [convert_args $method $args]

	puts "Rep0$tnum: Test of logs-only replication clients"

	replsetup $testdir/MSGQUEUEDIR
	set masterdir $testdir/MASTERDIR
	file mkdir $masterdir
	set clientdir $testdir/CLIENTDIR
	file mkdir $clientdir
	set logsonlydir $testdir/LOGSONLYDIR
	file mkdir $logsonlydir

	# Open a master, a logsonly replica, and a normal client.
	repladd 1
	set masterenv [berkdb_env -create -home $masterdir -txn -rep_master \
	    -rep_transport [list 1 replsend]]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	repladd 2
	set loenv [berkdb_env -create -home $logsonlydir -txn -rep_logsonly \
	    -rep_transport [list 2 replsend]]
	error_check_good logsonly_env [is_valid_env $loenv] TRUE

	repladd 3
	set clientenv [berkdb_env -create -home $clientdir -txn -rep_client \
	    -rep_transport [list 3 replsend]]
	error_check_good client_env [is_valid_env $clientenv] TRUE


	puts "\tRep0$tnum.a: Populate database."

	set db [eval {berkdb open -create -mode 0644 -auto_commit} \
	    -env $masterenv $oargs $omethod $dbname]
	error_check_good dbopen [is_valid_db $db] TRUE

	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $nitems } {
		if { [is_record_based $method] == 1 } {
			set key [expr $count + 1]
			set data $str
		} else {
			set key $str
			set data [reverse $str]
		}
		set kvals($count) $key
		set dvals($count) [pad_data $method $data]

		set txn [$masterenv txn]
		error_check_good txn($count) [is_valid_txn $txn $masterenv] TRUE

		set ret [eval \
		    {$db put} -txn $txn {$key [chop_data $method $data]}] 
		error_check_good put($count) $ret 0

		error_check_good commit($count) [$txn commit] 0

		incr count
	}

	puts "\tRep0$tnum.b: Sync up clients."
	set donenow 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $loenv 2]
		incr nproced [replprocessqueue $clientenv 3]

		if { $nproced == 0 } {
			break
		}
	}


	puts "\tRep0$tnum.c: Get master and logs-only client ahead."
	set newcount 0
	while { [gets $did str] != -1 && $newcount < $nitems } {
		if { [is_record_based $method] == 1 } {
			set key [expr $count + 1]
			set data $str
		} else {
			set key $str
			set data [reverse $str]
		}
		set kvals($count) $key
		set dvals($count) [pad_data $method $data]

		set txn [$masterenv txn]
		error_check_good txn($count) [is_valid_txn $txn $masterenv] TRUE

		set ret [eval \
		    {$db put} -txn $txn {$key [chop_data $method $data]}] 
		error_check_good put($count) $ret 0

		error_check_good commit($count) [$txn commit] 0

		incr count
		incr newcount
	}

	error_check_good db_close [$db close] 0

	puts "\tRep0$tnum.d: Sync up logs-only client only, then fail over."
	set donenow 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $loenv 2]

		if { $nproced == 0 } {
			break
		}
	}


	# "Crash" the master, and fail over to the upgradeable client.
	error_check_good masterenv_close [$masterenv close] 0
	replclear 3	

	error_check_good upgrade_client [$clientenv rep_start -master] 0
	set donenow 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $clientenv 3]
		incr nproced [replprocessqueue $loenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	error_check_good loenv_close [$loenv close] 0

	puts "\tRep0$tnum.e: Run catastrophic recovery on logs-only client."
	set loenv [berkdb_env -create -home $logsonlydir -txn -recover_fatal]

	puts "\tRep0$tnum.f: Verify logs-only client contents."
	set lodb [eval {berkdb open} -env $loenv $oargs $omethod $dbname]
	set loc [$lodb cursor]

	set cdb [eval {berkdb open} -env $clientenv $oargs $omethod $dbname]
	set cc [$cdb cursor]

	# Make sure new master and recovered logs-only replica match.
	for { set cdbt [$cc get -first] } \
	    { [llength $cdbt] > 0 } { set cdbt [$cc get -next] } {
		set lodbt [$loc get -next]

		error_check_good newmaster_replica_match $cdbt $lodbt
	}

	# Reset new master cursor.
	error_check_good cc_close [$cc close] 0
	set cc [$cdb cursor]
	
	for { set lodbt [$loc get -first] } \
	    { [llength $lodbt] > 0 } { set lodbt [$loc get -next] } {
		set cdbt [$cc get -next]

		error_check_good replica_newmaster_match $lodbt $cdbt
	}

	error_check_good loc_close [$loc close] 0
	error_check_good lodb_close [$lodb close] 0
	error_check_good loenv_close [$loenv close] 0

	error_check_good cc_close [$cc close] 0
	error_check_good cdb_close [$cdb close] 0
	error_check_good clientenv_close [$clientenv close] 0
	
	close $did

	replclose $testdir/MSGQUEUEDIR
}
