# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep001.tcl,v 1.16 2002/08/26 17:52:19 margo Exp $
#
# TEST  rep001
# TEST	Replication rename and forced-upgrade test.
# TEST
# TEST	Run a modified version of test001 in a replicated master environment;
# TEST  verify that the database on the client is correct.
# TEST	Next, remove the database, close the master, upgrade the 
# TEST	client, reopen the master, and make sure the new master can correctly
# TEST	run test001 and propagate it in the other direction.

proc rep001 { method { niter 1000 } { tnum "01" } args } {
	global passwd

	puts "Rep0$tnum: Replication sanity test."

	set envargs ""
	rep001_sub $method $niter $tnum $envargs $args

	puts "Rep0$tnum: Replication and security sanity test."
	append envargs " -encryptaes $passwd "
	append args " -encrypt "
	rep001_sub $method $niter $tnum $envargs $args
}

proc rep001_sub { method niter tnum envargs largs } {
	source ./include.tcl
	global testdir
	global encrypt

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR

	file mkdir $masterdir
	file mkdir $clientdir

	if { [is_record_based $method] == 1 } {
		set checkfunc test001_recno.check
	} else {
		set checkfunc test001.check
	}

	# Open a master.
	repladd 1
	set masterenv \
	    [eval {berkdb_env -create -lock_max 2500 -log_max 1000000} \
	    $envargs {-home $masterdir -txn -rep_master -rep_transport \
	    [list 1 replsend]}]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set clientenv [eval {berkdb_env -create} $envargs -txn -lock_max 2500 \
	    {-home $clientdir -rep_client -rep_transport [list 2 replsend]}]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Bring the client online by processing the startup messages.
	set donenow 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	# Open a test database on the master (so we can test having handles
	# open across an upgrade).
	puts "\tRep0$tnum.a:\
	    Opening test database for post-upgrade client logging test."
	set master_upg_db [berkdb_open \
	    -create -auto_commit -btree -env $masterenv rep0$tnum-upg.db]
	set puttxn [$masterenv txn]
	error_check_good master_upg_db_put \
	    [$master_upg_db put -txn $puttxn hello world] 0
	error_check_good puttxn_commit [$puttxn commit] 0
	error_check_good master_upg_db_close [$master_upg_db close] 0

	# Run a modified test001 in the master (and update client).
	puts "\tRep0$tnum.b: Running test001 in replicated env."
	eval test001 $method $niter 0 $tnum 1 -env $masterenv $largs
	set donenow 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	# Open the cross-upgrade database on the client and check its contents.
	set client_upg_db [berkdb_open \
	     -create -auto_commit -btree -env $clientenv rep0$tnum-upg.db]
	error_check_good client_upg_db_get [$client_upg_db get hello] \
	     [list [list hello world]]
	# !!! We use this handle later.  Don't close it here.

	# Verify the database in the client dir.
	puts "\tRep0$tnum.c: Verifying client database contents."
	set testdir [get_home $masterenv]
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	open_and_dump_file test0$tnum.db $clientenv $t1 \
	    $checkfunc dump_file_direction "-first" "-next"

	# Remove the file (and update client).
	puts "\tRep0$tnum.d: Remove the file on the master and close master."
	error_check_good remove \
	    [$masterenv dbremove -auto_commit test0$tnum.db] 0
	error_check_good masterenv_close [$masterenv close] 0
	set donenow 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]
		incr nproced [replprocessqueue $clientenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	# Don't get confused in Tcl.
	puts "\tRep0$tnum.e: Upgrade client."
	set newmasterenv $clientenv
	error_check_good upgrade_client [$newmasterenv rep_start -master] 0

	# Run test001 in the new master
	puts "\tRep0$tnum.f: Running test001 in new master."
	eval test001 $method $niter 0 $tnum 1 -env $newmasterenv $largs
	set donenow 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $newmasterenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	puts "\tRep0$tnum.g: Reopen old master as client and catch up."
	# Throttle master so it can't send everything at once
	$newmasterenv rep_limit 0 [expr 64 * 1024]
	set newclientenv [eval {berkdb_env -create -recover} $envargs \
	    -txn -lock_max 2500 \
	    {-home $masterdir -rep_client -rep_transport [list 1 replsend]}]
	error_check_good newclient_env [is_valid_env $newclientenv] TRUE
	set donenow 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $newclientenv 1]
		incr nproced [replprocessqueue $newmasterenv 2]

		if { $nproced == 0 } {
			break
		}
	}
	set stats [$newmasterenv rep_stat]
	set nthrottles [getstats $stats {Transmission limited}]
	error_check_bad nthrottles $nthrottles -1
	error_check_bad nthrottles $nthrottles 0

	# Run a modified test001 in the new master (and update client).
	puts "\tRep0$tnum.h: Running test001 in new master."
	eval test001 $method \
	    $niter $niter $tnum 1 -env $newmasterenv $largs
	set donenow 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $newclientenv 1]
		incr nproced [replprocessqueue $newmasterenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	# Test put to the database handle we opened back when the new master
	# was a client.
	puts "\tRep0$tnum.i: Test put to handle opened before upgrade."
	set puttxn [$newmasterenv txn]
	error_check_good client_upg_db_put \
	    [$client_upg_db put -txn $puttxn hello there] 0
	error_check_good puttxn_commit [$puttxn commit] 0
	set donenow 0
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $newclientenv 1]
		incr nproced [replprocessqueue $newmasterenv 2]

		if { $nproced == 0 } {
			break
		}
	}

	# Close the new master's handle for the upgrade-test database;  we
	# don't need it.  Then check to make sure the client did in fact
	# update the database.
	error_check_good client_upg_db_close [$client_upg_db close] 0
	set newclient_upg_db [berkdb_open -env $newclientenv rep0$tnum-upg.db]
	error_check_good newclient_upg_db_get [$newclient_upg_db get hello] \
	    [list [list hello there]]
	error_check_good newclient_upg_db_close [$newclient_upg_db close] 0

	# Verify the database in the client dir.
	puts "\tRep0$tnum.j: Verifying new client database contents."
	set testdir [get_home $newmasterenv]
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	open_and_dump_file test0$tnum.db $newclientenv $t1 \
	    $checkfunc dump_file_direction "-first" "-next"

	if { [string compare [convert_method $method] -recno] != 0 } {
		filesort $t1 $t3
	}
	error_check_good diff_files($t2,$t3) [filecmp $t2 $t3] 0


	error_check_good newmasterenv_close [$newmasterenv close] 0
	error_check_good newclientenv_close [$newclientenv close] 0

	if { [lsearch $envargs "-encrypta*"] !=-1 } {
		set encrypt 1
	}
	error_check_good verify \
	    [verify_dir $clientdir "\tRep0$tnum.k: " 0 0 1] 0
	replclose $testdir/MSGQUEUEDIR
}
