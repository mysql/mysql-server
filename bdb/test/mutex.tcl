# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: mutex.tcl,v 11.18 2000/09/01 19:24:59 krinsky Exp $
#
# Exercise mutex functionality.
# Options are:
# -dir <directory in which to store mpool>
# -iter <iterations>
# -mdegree <number of mutexes per iteration>
# -nmutex <number of mutexes>
# -procs <number of processes to run>
# -wait <wait interval after getting locks>
proc mutex_usage {} {
	puts stderr "mutex\n\t-dir <dir>\n\t-iter <iterations>"
	puts stderr "\t-mdegree <locks per iteration>\n\t-nmutex <n>"
	puts stderr "\t-procs <nprocs>"
	puts stderr "\n\t-wait <max wait interval>"
	return
}

proc mutex { args } {
	source ./include.tcl

	set dir db
	set iter 500
	set mdegree 3
	set nmutex 20
	set procs 5
	set wait 2

	for { set i 0 } { $i < [llength $args] } {incr i} {
		switch -regexp -- [lindex $args $i] {
			-d.* { incr i; set testdir [lindex $args $i] }
			-i.* { incr i; set iter [lindex $args $i] }
			-m.* { incr i; set mdegree [lindex $args $i] }
			-n.* { incr i; set nmutex [lindex $args $i] }
			-p.* { incr i; set procs [lindex $args $i] }
			-w.* { incr i; set wait [lindex $args $i] }
			default {
				puts -nonewline "FAIL:[timestamp] Usage: "
				mutex_usage
				return
			}
		}
	}

	if { [file exists $testdir/$dir] != 1 } {
		file mkdir $testdir/$dir
	} elseif { [file isdirectory $testdir/$dir ] != 1 } {
		error "$testdir/$dir is not a directory"
	}

	# Basic sanity tests
	mutex001 $testdir $nmutex

	# Basic synchronization tests
	mutex002 $testdir $nmutex

	# Multiprocess tests
	mutex003 $testdir $iter $nmutex $procs $mdegree $wait
}

proc mutex001 { dir nlocks } {
	source ./include.tcl

	puts "Mutex001: Basic functionality"
	env_cleanup $dir

	# Test open w/out create; should fail
	error_check_bad \
	    env_open [catch {berkdb env -lock -home $dir} env] 0

	# Now open for real
	set env [berkdb env -create -mode 0644 -lock -home $dir]
	error_check_good env_open [is_valid_env $env] TRUE

	set m [$env mutex 0644 $nlocks]
	error_check_good mutex_init [is_valid_mutex $m $env] TRUE

	# Get, set each mutex; sleep, then get Release
	for { set i 0 } { $i < $nlocks } { incr i } {
		set r [$m get $i ]
		error_check_good mutex_get $r 0

		set r [$m setval $i $i]
		error_check_good mutex_setval $r 0
	}
	tclsleep 5
	for { set i 0 } { $i < $nlocks } { incr i } {
		set r [$m getval $i]
		error_check_good mutex_getval $r $i

		set r [$m release $i ]
		error_check_good mutex_get $r 0
	}

	error_check_good mutex_close [$m close] 0
	error_check_good env_close [$env close] 0
	puts "Mutex001: completed successfully."
}

# Test basic synchronization
proc mutex002 { dir nlocks } {
	source ./include.tcl

	puts "Mutex002: Basic synchronization"
	env_cleanup $dir

	# Fork off child before we open any files.
	set f1 [open |$tclsh_path r+]
	puts $f1 "source $test_path/test.tcl"
	flush $f1

	# Open the environment and the mutex locally
	set local_env [berkdb env -create -mode 0644 -lock -home $dir]
	error_check_good env_open [is_valid_env $local_env] TRUE

	set local_mutex [$local_env mutex 0644 $nlocks]
	error_check_good \
	    mutex_init [is_valid_mutex $local_mutex $local_env] TRUE

	# Open the environment and the mutex remotely
	set remote_env [send_cmd $f1 "berkdb env -lock -home $dir"]
	error_check_good remote:env_open [is_valid_env $remote_env] TRUE

	set remote_mutex [send_cmd $f1 "$remote_env mutex 0644 $nlocks"]
	error_check_good \
	    mutex_init [is_valid_mutex $remote_mutex $remote_env] TRUE

	# Do a get here, then set the value to be pid.
	# On the remote side fire off a get and getval.
	set r [$local_mutex get 1]
	error_check_good lock_get $r 0

	set r [$local_mutex setval 1 [pid]]
	error_check_good lock_get $r 0

	# Now have the remote side request the lock and check its
	# value. Then wait 5 seconds, release the mutex and see
	# what the remote side returned.
	send_timed_cmd $f1 1 "$remote_mutex get 1"
	send_timed_cmd $f1 1 "set ret \[$remote_mutex getval 1\]"

	# Now sleep before resetting and releasing lock
	tclsleep 5
	set newv [expr [pid] - 1]
	set r [$local_mutex setval 1 $newv]
	error_check_good mutex_setval $r 0

	set r [$local_mutex release 1]
	error_check_good mutex_release $r 0

	# Now get the result from the other script
	# Timestamp
	set result [rcv_result $f1]
	error_check_good lock_get:remote_time [expr $result > 4] 1

	# Timestamp
	set result [rcv_result $f1]

	# Mutex value
	set result [send_cmd $f1 "puts \$ret"]
	error_check_good lock_get:remote_getval $result $newv

	# Close down the remote
	set ret [send_cmd $f1 "$remote_mutex close" 5]
	# Not sure why we need this, but we do... an extra blank line
	# someone gets output somewhere
	gets $f1 ret
	error_check_good remote:mutex_close $ret 0

	set ret [send_cmd $f1 "$remote_env close"]
	error_check_good remote:env_close $ret 0

	catch { close $f1 } result

	set ret [$local_mutex close]
	error_check_good local:mutex_close $ret 0

	set ret [$local_env close]
	error_check_good local:env_close $ret 0

	puts "Mutex002: completed successfully."
}

# Generate a bunch of parallel
# testers that try to randomly obtain locks.
proc mutex003 { dir iter nmutex procs mdegree wait } {
	source ./include.tcl

	puts "Mutex003: Multi-process random mutex test ($procs processes)"

	env_cleanup $dir

	# Now open the region we'll use for multiprocess testing.
	set env [berkdb env -create -mode 0644 -lock -home $dir]
	error_check_good env_open [is_valid_env $env] TRUE

	set mutex [$env mutex 0644 $nmutex]
	error_check_good mutex_init [is_valid_mutex $mutex $env] TRUE

	error_check_good mutex_close [$mutex close] 0

	# Now spawn off processes
	set proclist {}
	for { set i 0 } {$i < $procs} {incr i} {
		puts "$tclsh_path\
		    $test_path/mutexscript.tcl $dir\
		    $iter $nmutex $wait $mdegree > $testdir/$i.mutexout &"
		set p [exec $tclsh_path $test_path/wrap.tcl \
		    mutexscript.tcl $testdir/$i.mutexout $dir\
		    $iter $nmutex $wait $mdegree &]
		lappend proclist $p
	}
	puts "Mutex003: $procs independent processes now running"
	watch_procs
	error_check_good env_close [$env close] 0
	# Remove output files
	for { set i 0 } {$i < $procs} {incr i} {
		fileremove -f $dir/$i.mutexout
	}
}
