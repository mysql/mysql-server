# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: mutex003.tcl,v 11.26 2004/01/28 03:36:28 bostic Exp $
#

# TEST	mutex003
# TEST	Generate a bunch of parallel testers that try to randomly obtain locks.
proc mutex003 { } {
	source ./include.tcl

	set nmutex 20
	set iter 500
	set procs 5
	set mdegree 3
	set wait 2
	puts "Mutex003: Multi-process random mutex test"

	env_cleanup $testdir

	puts "\tMutex003.a: Create environment"
	# Now open the region we'll use for multiprocess testing.
	set env [berkdb_env -create -mode 0644 -lock -home $testdir]
	error_check_good env_open [is_valid_env $env] TRUE

	set mutex [$env mutex 0644 $nmutex]
	error_check_good mutex_init [is_valid_mutex $mutex $env] TRUE

	error_check_good mutex_close [$mutex close] 0

	# Now spawn off processes
	puts "\tMutex003.b: Create $procs processes"
	set pidlist {}
	for { set i 0 } {$i < $procs} {incr i} {
		puts "$tclsh_path\
		    $test_path/mutexscript.tcl $testdir\
		    $iter $nmutex $wait $mdegree > $testdir/$i.mutexout &"
		set p [exec $tclsh_path $test_path/wrap.tcl \
		    mutexscript.tcl $testdir/$i.mutexout $testdir\
		    $iter $nmutex $wait $mdegree &]
		lappend pidlist $p
	}
	puts "\tMutex003.c: $procs independent processes now running"
	watch_procs $pidlist
	error_check_good env_close [$env close] 0
	# Remove output files
	for { set i 0 } {$i < $procs} {incr i} {
		fileremove -f $testdir/$i.mutexout
	}
}
