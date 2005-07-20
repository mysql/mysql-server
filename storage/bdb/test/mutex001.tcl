# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: mutex001.tcl,v 11.25 2004/01/28 03:36:28 bostic Exp $
#

# TEST	mutex001
# TEST	Test basic mutex functionality
proc mutex001 { } {
	source ./include.tcl

	puts "Mutex001: Basic functionality"
	env_cleanup $testdir
	set nlocks 20

	# Test open w/out create; should fail
	error_check_bad \
	    env_open [catch {berkdb_env -lock -home $testdir} env] 0

	puts "\tMutex001.a: Create lock env"
	# Now open for real
	set env [berkdb_env -create -mode 0644 -lock -home $testdir]
	error_check_good env_open [is_valid_env $env] TRUE

	puts "\tMutex001.b: Create $nlocks mutexes"
	set m [$env mutex 0644 $nlocks]
	error_check_good mutex_init [is_valid_mutex $m $env] TRUE

	# Get, set each mutex; sleep, then get Release
	puts "\tMutex001.c: Get/set loop"
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
}
