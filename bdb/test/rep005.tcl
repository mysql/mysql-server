# See the file LICENSE for redistribution information.
#
# Copyright (c) 2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep005.tcl,v 11.3 2002/08/08 18:13:13 sue Exp $
#
# TEST  rep005
# TEST	Replication election test with error handling.
# TEST
# TEST	Run a modified version of test001 in a replicated master environment;
# TEST  hold an election among a group of clients to make sure they select
# TEST  a proper master from amongst themselves, forcing errors at various
# TEST	locations in the election path.

proc rep005 { method { niter 10 } { tnum "05" } args } {
	source ./include.tcl

	if { [is_record_based $method] == 1 } {
		puts "Rep005: Skipping for method $method."
		return
	}

	set nclients 3
	env_cleanup $testdir

	set qdir $testdir/MSGQUEUEDIR
	replsetup $qdir

	set masterdir $testdir/MASTERDIR
	file mkdir $masterdir

	for { set i 0 } { $i < $nclients } { incr i } {
		set clientdir($i) $testdir/CLIENTDIR.$i
		file mkdir $clientdir($i)
	}

	puts "Rep0$tnum: Replication election test with $nclients clients."

	# Open a master.
	repladd 1
	set env_cmd(M) "berkdb_env -create -log_max 1000000 -home \
	    $masterdir -txn -rep_master -rep_transport \[list 1 replsend\]"
	set masterenv [eval $env_cmd(M)]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open the clients.
	for { set i 0 } { $i < $nclients } { incr i } {
		set envid [expr $i + 2]
		repladd $envid
		set env_cmd($i) "berkdb_env -create -home $clientdir($i) \
		    -txn -rep_client -rep_transport \[list $envid replsend\]"
		set clientenv($i) [eval $env_cmd($i)]
		error_check_good \
		    client_env($i) [is_valid_env $clientenv($i)] TRUE
	}

	# Run a modified test001 in the master.
	puts "\tRep0$tnum.a: Running test001 in replicated env."
	eval test001 $method $niter 0 $tnum 0 -env $masterenv $args

	# Loop, processing first the master's messages, then the client's,
	# until both queues are empty.
	while { 1 } {
		set nproced 0

		incr nproced [replprocessqueue $masterenv 1]

		for { set i 0 } { $i < $nclients } { incr i } { 	
			set envid [expr $i + 2]
			incr nproced [replprocessqueue $clientenv($i) $envid]
		}

		if { $nproced == 0 } {
			break
		}
	}

	# Verify the database in the client dir.
	for { set i 0 } { $i < $nclients } { incr i } {
		puts "\tRep0$tnum.b: Verifying contents of client database $i."
		set testdir [get_home $masterenv]
		set t1 $testdir/t1
		set t2 $testdir/t2
		set t3 $testdir/t3
		open_and_dump_file test0$tnum.db $clientenv($i) $testdir/t1 \
	    	    test001.check dump_file_direction "-first" "-next"

		if { [string compare [convert_method $method] -recno] != 0 } {
			filesort $t1 $t3
		}
		error_check_good diff_files($t2,$t3) [filecmp $t2 $t3] 0

		verify_dir $clientdir($i) "\tRep0$tnum.c: " 0 0 1
	}

	# Make sure all the clients are synced up and ready to be good
	# voting citizens.
	error_check_good master_flush [$masterenv rep_flush] 0
	while { 1 } {
		set nproced 0
		incr nproced [replprocessqueue $masterenv 1 0]
		for { set i 0 } { $i < $nclients } { incr i } {
			incr nproced [replprocessqueue $clientenv($i) \
			    [expr $i + 2] 0]
		}

		if { $nproced == 0 } {
			break
		}
	}

	error_check_good masterenv_close [$masterenv close] 0

	for { set i 0 } { $i < $nclients } { incr i } {
		replclear [expr $i + 2]
	}
	#
	# We set up the error list for each client.  We know that the
	# first client is the one calling the election, therefore, add
	# the error location on sending the message (electsend) for that one.
	set m "Rep0$tnum"
	set count 0
	foreach c0 { electinit electsend electvote1 electwait1 electvote2 \
	    electwait2 } {
		foreach c1 { electinit electvote1 electwait1 electvote2 \
		    electwait2 } {
			foreach c2 { electinit electvote1 electwait1 \
			    electvote2 electwait2 } {
				set elist [list $c0 $c1 $c2]
				rep005_elect env_cmd clientenv $qdir $m \
				    $count $elist
				incr count
			}
		}
	}

	for { set i 0 } { $i < $nclients } { incr i } {
		error_check_good clientenv_close($i) [$clientenv($i) close] 0
	}

	replclose $testdir/MSGQUEUEDIR
}

proc rep005_elect { ecmd cenv qdir msg count elist } {
	global elect_timeout
	upvar $ecmd env_cmd
	upvar $cenv clientenv

	set elect_timeout 1000000
	set nclients [llength $elist]

	for { set i 0 } { $i < $nclients } { incr i } { 	
		set err_cmd($i) [lindex $elist $i]
	}
	puts "\t$msg.d.$count: Starting election with errors $elist"
	set elect_pipe(0) [start_election $qdir $env_cmd(0) \
	    [expr $nclients + 1] 20 $elect_timeout $err_cmd(0)]

	tclsleep 1

	# Process messages, and verify that the client with the highest
	# priority--client #1--wins.
	set got_newmaster 0
	set tries 10
	while { 1 } {
		set nproced 0
		set he 0
		set nm 0
		
		for { set i 0 } { $i < $nclients } { incr i } { 	
			set he 0
			set envid [expr $i + 2]
# puts "Processing queue for client $i"
			incr nproced \
			    [replprocessqueue $clientenv($i) $envid 0 he nm]
			if { $he == 1 } {
				# Client #1 has priority 100;  everyone else
				if { $i == 1 } {
					set pri 100
				} else {
					set pri 10
				}
				# error_check_bad client(0)_in_elect $i 0
# puts "Starting election on client $i"
				set elect_pipe($i) [start_election $qdir \
			    	    $env_cmd($i) [expr $nclients + 1] $pri \
				    $elect_timeout $err_cmd($i)]
				set got_hold_elect($i) 1
			}
			if { $nm != 0 } {
				error_check_good newmaster_is_master $nm \
				    [expr 1 + 2]
				set got_newmaster $nm

				# If this env is the new master, it needs to
				# configure itself as such--this is a different
				# env handle from the one that performed the
				# election.
				if { $nm == $envid } {
					error_check_good make_master($i) \
					    [$clientenv($i) rep_start -master] \
					    0
				}
			}
		}

		# We need to wait around to make doubly sure that the
		# election has finished...
		if { $nproced == 0 } {
			incr tries -1
			if { $tries == 0 } {
				break
			} else {
				tclsleep 1
			}
		}
	}

	# Verify that client #1 is actually the winner.
	error_check_good "client 1 wins" $got_newmaster [expr 1 + 2]

	cleanup_elections

}
