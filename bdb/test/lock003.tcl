# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: lock003.tcl,v 11.16 2000/08/25 14:21:51 sue Exp $
#
# Exercise multi-process aspects of lock.  Generate a bunch of parallel
# testers that try to randomly obtain locks.
proc lock003 { dir {iter 500} {max 1000} {procs 5} {ldegree 5} {objs 75} \
	{reads 65} {wait 1} {conflicts { 3 0 0 0 0 0 1 0 1 1}} {seeds {}} } {
	source ./include.tcl

	puts "Lock003: Multi-process random lock test"

	# Clean up after previous runs
	env_cleanup $dir

	# Open/create the lock region
	set e [berkdb env -create -lock -home $dir]
	error_check_good env_open [is_substr $e env] 1

	set ret [$e close]
	error_check_good env_close $ret 0

	# Now spawn off processes
	set pidlist {}
	for { set i 0 } {$i < $procs} {incr i} {
		if { [llength $seeds] == $procs } {
			set s [lindex $seeds $i]
		}
		puts "$tclsh_path\
		    $test_path/wrap.tcl \
		    lockscript.tcl $dir/$i.lockout\
		    $dir $iter $objs $wait $ldegree $reads &"
		set p [exec $tclsh_path $test_path/wrap.tcl \
		    lockscript.tcl $testdir/lock003.$i.out \
		    $dir $iter $objs $wait $ldegree $reads &]
		lappend pidlist $p
	}

	puts "Lock003: $procs independent processes now running"
	watch_procs 30 10800
	# Remove log files
	for { set i 0 } {$i < $procs} {incr i} {
		fileremove -f $dir/$i.lockout
	}
}
