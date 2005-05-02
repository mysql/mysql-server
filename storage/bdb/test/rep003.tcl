# See the file LICENSE for redistribution information.
#
# Copyright (c) 2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep003.tcl,v 11.9 2002/08/09 02:23:50 margo Exp $
#
# TEST  rep003
# TEST	Repeated shutdown/restart replication test
# TEST
# TEST	Run a quick put test in a replicated master environment;  start up, 
# TEST  shut down, and restart client processes, with and without recovery.
# TEST  To ensure that environment state is transient, use DB_PRIVATE.

proc rep003 { method { tnum "03" } args } {
	source ./include.tcl
	global testdir rep003_dbname rep003_omethod rep003_oargs

	env_cleanup $testdir
	set niter 10
	set rep003_dbname rep003.db

	if { [is_record_based $method] } {
		puts "Rep0$tnum: Skipping for method $method"
		return
	}

	set rep003_omethod [convert_method $method]
	set rep003_oargs [convert_args $method $args]

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	file mkdir $masterdir
	
	set clientdir $testdir/CLIENTDIR
	file mkdir $clientdir

	puts "Rep0$tnum: Replication repeated-startup test"

	# Open a master.
	repladd 1
	set masterenv [berkdb_env_noerr -create -log_max 1000000 \
	    -home $masterdir -txn -rep_master -rep_transport [list 1 replsend]]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	puts "\tRep0$tnum.a: Simple client startup test."

	# Put item one.
	rep003_put $masterenv A1 a-one

	# Open a client.
	repladd 2
	set clientenv [berkdb_env_noerr -create -private -home $clientdir -txn \
	    -rep_client -rep_transport [list 2 replsend]]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Put another quick item.
	rep003_put $masterenv A2 a-two

	# Loop, processing first the master's messages, then the client's,
	# until both queues are empty.
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	rep003_check $clientenv A1 a-one
	rep003_check $clientenv A2 a-two

	error_check_good clientenv_close [$clientenv close] 0
	replclear 2

	# Now reopen the client after doing another put.
	puts "\tRep0$tnum.b: Client restart."
	rep003_put $masterenv B1 b-one

	unset clientenv
	set clientenv [berkdb_env_noerr -create -private -home $clientdir -txn \
	    -rep_client -rep_transport [list 2 replsend]]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	rep003_put $masterenv B2 b-two

	# Loop, processing first the master's messages, then the client's,
	# until both queues are empty.
	while { 1 } {
		set nproced 0

		# The items from part A should be present at all times--
		# if we roll them back, we've screwed up. [#5709]
		rep003_check $clientenv A1 a-one
		rep003_check $clientenv A2 a-two

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	rep003_check $clientenv B1 b-one
	rep003_check $clientenv B2 b-two

	error_check_good clientenv_close [$clientenv close] 0

	replclear 2

	# Now reopen the client after a recovery.
	puts "\tRep0$tnum.c: Client restart after recovery."
	rep003_put $masterenv C1 c-one

	unset clientenv
	set clientenv [berkdb_env_noerr -create -private -home $clientdir -txn \
	    -recover -rep_client -rep_transport [list 2 replsend]]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	rep003_put $masterenv C2 c-two

	# Loop, processing first the master's messages, then the client's,
	# until both queues are empty.
	while { 1 } {
		set nproced 0

		# The items from part A should be present at all times--
		# if we roll them back, we've screwed up. [#5709]
		rep003_check $clientenv A1 a-one
		rep003_check $clientenv A2 a-two
		rep003_check $clientenv B1 b-one 
		rep003_check $clientenv B2 b-two

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	rep003_check $clientenv C1 c-one
	rep003_check $clientenv C2 c-two

	error_check_good clientenv_close [$clientenv close] 0

	replclear 2

	# Now reopen the client after a catastrophic recovery.
	puts "\tRep0$tnum.d: Client restart after catastrophic recovery."
	rep003_put $masterenv D1 d-one

	unset clientenv
	set clientenv [berkdb_env_noerr -create -private -home $clientdir -txn \
	    -recover_fatal -rep_client -rep_transport [list 2 replsend]]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	rep003_put $masterenv D2 d-two

	# Loop, processing first the master's messages, then the client's,
	# until both queues are empty.
	while { 1 } {
		set nproced 0

		# The items from part A should be present at all times--
		# if we roll them back, we've screwed up. [#5709]
		rep003_check $clientenv A1 a-one
		rep003_check $clientenv A2 a-two
		rep003_check $clientenv B1 b-one 
		rep003_check $clientenv B2 b-two
		rep003_check $clientenv C1 c-one 
		rep003_check $clientenv C2 c-two

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	rep003_check $clientenv D1 d-one
	rep003_check $clientenv D2 d-two

	error_check_good clientenv_close [$clientenv close] 0

	error_check_good masterenv_close [$masterenv close] 0
	replclose $testdir/MSGQUEUEDIR
}

proc rep003_put { masterenv key data } {
	global rep003_dbname rep003_omethod rep003_oargs

	set db [eval {berkdb_open_noerr -create -env $masterenv -auto_commit} \
	    $rep003_omethod $rep003_oargs $rep003_dbname]
	error_check_good rep3_put_open($key,$data) [is_valid_db $db] TRUE

	set txn [$masterenv txn]
	error_check_good rep3_put($key,$data) [$db put -txn $txn $key $data] 0
	error_check_good rep3_put_txn_commit($key,$data) [$txn commit] 0

	error_check_good rep3_put_close($key,$data) [$db close] 0
}

proc rep003_check { env key data } {
	global rep003_dbname

	set db [berkdb_open_noerr -rdonly -env $env $rep003_dbname]
	error_check_good rep3_check_open($key,$data) [is_valid_db $db] TRUE

	set dbt [$db get $key]
	error_check_good rep3_check($key,$data) \
	    [lindex [lindex $dbt 0] 1] $data

	error_check_good rep3_put_close($key,$data) [$db close] 0
}
