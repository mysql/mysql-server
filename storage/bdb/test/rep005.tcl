# See the file LICENSE for redistribution information.
#
# Copyright (c) 2002-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: rep005.tcl,v 11.41 2004/10/15 15:41:56 sue Exp $
#
# TEST  rep005
# TEST	Replication election test with error handling.
# TEST
# TEST	Run a modified version of test001 in a replicated master environment;
# TEST  hold an election among a group of clients to make sure they select
# TEST  a proper master from amongst themselves, forcing errors at various
# TEST	locations in the election path.

proc rep005 { method args } {
	if { [is_btree $method] == 0 } {
		puts "Rep005: Skipping for method $method."
		return
	}

	set tnum "005"
	set niter 10
	set nclients 3
	set logsets [create_logsets [expr $nclients + 1]]

	# We don't want to run this with -recover - it takes too
	# long and doesn't cover any new ground.
	set recargs ""
	foreach l $logsets {
		puts "Rep$tnum ($recargs): Replication election\
		    error test with $nclients clients."
		puts -nonewline "Rep$tnum: Started at: "
		puts [clock format [clock seconds] -format "%H:%M %D"]
		puts "Rep$tnum: Master logs are [lindex $l 0]"
		for { set i 0 } { $i < $nclients } { incr i } {
			puts "Rep$tnum: Client $i logs are\
			    [lindex $l [expr $i + 1]]"
		}
		rep005_sub $method $tnum \
		    $niter $nclients $l $recargs $args
	}
}

proc rep005_sub { method tnum niter nclients logset recargs largs } {
	source ./include.tcl
	global rand_init
	error_check_good set_random_seed [berkdb srand $rand_init] 0

	env_cleanup $testdir

	set qdir $testdir/MSGQUEUEDIR
	replsetup $qdir

	set masterdir $testdir/MASTERDIR
	file mkdir $masterdir
	set m_logtype [lindex $logset 0]
	set m_logargs [adjust_logargs $m_logtype]
	set m_txnargs [adjust_txnargs $m_logtype]

	for { set i 0 } { $i < $nclients } { incr i } {
		set clientdir($i) $testdir/CLIENTDIR.$i
		file mkdir $clientdir($i)
		set c_logtype($i) [lindex $logset [expr $i + 1]]
		set c_logargs($i) [adjust_logargs $c_logtype($i)]
		set c_txnargs($i) [adjust_txnargs $c_logtype($i)]
	}

	# Open a master.
	repladd 1
	set env_cmd(M) "berkdb_env -create -log_max 1000000 \
	    -home $masterdir $m_logargs \
	    $m_txnargs -rep_master -rep_transport \[list 1 replsend\]"
# To debug elections, uncomment the line below and further below
# for the clients to turn on verbose.  Also edit reputils.tcl
# in proc start_election and swap the 2 commented lines with
# their counterpart.
#	set env_cmd(M) "berkdb_env_noerr -create -log_max 1000000 \
#	    -home $masterdir $m_logargs \
#	    $m_txnargs -rep_master \
#	    -verbose {rep on} -errpfx MASTER -errfile /dev/stderr \
#	    -rep_transport \[list 1 replsend\]"
	set masterenv [eval $env_cmd(M) $recargs]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	set envlist {}
	lappend envlist "$masterenv 1"

	# Open the clients.
	for { set i 0 } { $i < $nclients } { incr i } {
		set envid [expr $i + 2]
		repladd $envid
		set env_cmd($i) "berkdb_env -create -home $clientdir($i) \
		    $c_logargs($i) $c_txnargs($i) -rep_client \
		    -rep_transport \[list $envid replsend\]"
#		set env_cmd($i) "berkdb_env_noerr -create -home $clientdir($i) \
#		    -verbose {rep on} -errpfx CLIENT$i -errfile /dev/stderr \
#		    $c_logargs($i) $c_txnargs($i) -rep_client \
#		    -rep_transport \[list $envid replsend\]"
		set clientenv($i) [eval $env_cmd($i) $recargs]
		error_check_good \
		    client_env($i) [is_valid_env $clientenv($i)] TRUE
		lappend envlist "$clientenv($i) $envid"
	}

	# Run a modified test001 in the master.
	puts "\tRep$tnum.a: Running test001 in replicated env."
	eval rep_test $method $masterenv NULL $niter 0 0 0 $largs

	# Process all the messages and close the master.
	process_msgs $envlist
	error_check_good masterenv_close [$masterenv close] 0
	set envlist [lreplace $envlist 0 0]

	for { set i 0 } { $i < $nclients } { incr i } {
		replclear [expr $i + 2]
	}
	#
	# We set up the error list for each client.  We know that the
	# first client is the one calling the election, therefore, add
	# the error location on sending the message (electsend) for that one.
	set m "Rep$tnum"
	set count 0
	set win -1
	#
	# A full test can take a long time to run.  For normal testing
	# pare it down a lot so that it runs in a shorter time.
	#
	set c0err { none electinit none none }
	set c1err $c0err
	set c2err $c0err
	set numtests [expr [llength $c0err] * [llength $c1err] * \
	    [llength $c2err]]
	puts "\t$m.b: Starting $numtests election with error tests"
	set last_win -1
	set win -1
	foreach c0 $c0err {
		foreach c1 $c1err {
			foreach c2 $c2err {
				set elist [list $c0 $c1 $c2]
				rep005_elect env_cmd envlist $qdir \
				    $m $count win last_win $elist $logset
				incr count
			}
		}
	}

	foreach pair $envlist {
		set cenv [lindex $pair 0]
		error_check_good cenv_close [$cenv close] 0
	}

	replclose $testdir/MSGQUEUEDIR
	puts -nonewline \
	    "Rep$tnum: Completed at: "
	puts [clock format [clock seconds] -format "%H:%M %D"]
}

proc rep005_elect { ecmd celist qdir msg count \
    winner lsn_lose elist logset} {
	global elect_timeout elect_serial
	global is_windows_test
	upvar $ecmd env_cmd
	upvar $celist envlist
	upvar $winner win
	upvar $lsn_lose last_win

	set elect_timeout 5000000
	set nclients [llength $elist]
	set nsites [expr $nclients + 1]

	set cl_list {}
	foreach pair $envlist {
		set id [lindex $pair 1]
		set i [expr $id - 2]
		set clientenv($i) [lindex $pair 0]
		set err_cmd($i) [lindex $elist $i]
		set elect_pipe($i) INVALID
		replclear $id
		lappend cl_list $i
	}

	# Select winner.  We want to test biggest LSN wins, and secondarily
	# highest priority wins.  If we already have a master, make sure
	# we don't start a client in that master.
	set el 0
	if { $win == -1 } {
		if { $last_win != -1 } {
			set cl_list [lreplace $cl_list $last_win $last_win]
			set el $last_win
		}
		set windex [berkdb random_int 0 [expr [llength $cl_list] - 1]]
		set win [lindex $cl_list $windex]
	} else {
		# Easy case, if we have a master, the winner must be the
		# same one as last time, just use $win.
		# If client0 is the current existing master, start the
		# election in client 1.
		if {$win == 0} {
			set el 1
		}
	}
	# Winner has priority 100.  If we are testing LSN winning, the
	# make sure the lowest LSN client has the highest priority.
	# Everyone else has priority 10.
	for { set i 0 } { $i < $nclients } { incr i } {
		set crash($i) 0
		if { $i == $win } {
			set pri($i) 100
		} elseif { $i == $last_win } {
			set pri($i) 200
		} else {
			set pri($i) 10
		}
	}

	puts "\t$msg.b.$count: Start election (win=client$win) $elist"
	set msg $msg.c.$count
	set nsites $nclients
	set nvotes $nsites
	run_election env_cmd envlist err_cmd pri crash \
	    $qdir $msg $el $nsites $nvotes $nclients $win
	#
	# Sometimes test elections with an existing master.
	# Other times test elections without master by closing the
	# master we just elected and creating a new client.
	# We want to weight it to close the new master.  So, use
	# a list to cause closing about 70% of the time.
	#
	set close_list { 0 0 0 1 1 1 1 1 1 1}
	set close_len [expr [llength $close_list] - 1]
	set close_index [berkdb random_int 0 $close_len]
	if { [lindex $close_list $close_index] == 1 } {
		puts -nonewline "\t\t$msg: Closing "
		error_check_good newmaster_close [$clientenv($win) close] 0
		#
		# If the next test should win via LSN then remove the
		# env before starting the new client so that we
		# can guarantee this client doesn't win the next one.
		set lsn_win { 0 0 0 0 1 1 1 1 1 1 }
		set lsn_len [expr [llength $lsn_win] - 1]
		set lsn_index [berkdb random_int 0 $lsn_len]
		set rec_arg ""
		set win_inmem [expr [string compare [lindex $logset \
		    [expr $win + 1]] in-memory] == 0]
		if { [lindex $lsn_win $lsn_index] == 1 } {
			set last_win $win
			set dirindex [lsearch -exact $env_cmd($win) "-home"]
			incr dirindex
			set lsn_dir [lindex $env_cmd($win) $dirindex]
			env_cleanup $lsn_dir
			puts -nonewline "and cleaning "
		} else {
			#
			# If we're not cleaning the env, decide if we should
			# run recovery upon reopening the env.  This causes
			# two things:
			# 1. Removal of region files which forces the env
			# to read its __db.rep.egen file.
			# 2. Adding a couple log records, so this client must
			# be the next winner as well since it'll have the
			# biggest LSN.
			#
			set rec_win { 0 0 0 0 0 0 1 1 1 1 }
			set rec_len [expr [llength $rec_win] - 1]
			set rec_index [berkdb random_int 0 $rec_len]
			if { [lindex $rec_win $rec_index] == 1 } {
				puts -nonewline "and recovering "
				set rec_arg "-recover"
				#
				# If we're in memory and about to run
				# recovery, we force ourselves not to win
				# the next election because recovery will
				# blow away the entire log in memory.
				# However, we don't skip this entirely
				# because we still want to force reading
				# of __db.rep.egen.
				#
				if { $win_inmem } {
					set last_win $win
				} else {
					set last_win -1
				}
			} else {
				set last_win -1
			}
		}
		puts "new master, new client $win"
		set clientenv($win) [eval $env_cmd($win) $rec_arg]
		error_check_good cl($win) [is_valid_env $clientenv($win)] TRUE
		#
		# Since we started a new client, we need to replace it
		# in the message processing list so that we get the
		# new Tcl handle name in there.
		set newel "$clientenv($win) [expr $win + 2]"
		set envlist [lreplace $envlist $win $win $newel]
		if { $rec_arg == "" || $win_inmem } {
			set win -1
		}
		#
		# Since we started a new client we want to give them
		# all a chance to process everything outstanding before
		# the election on the next iteration.
		#
		process_msgs $envlist
	}
}
