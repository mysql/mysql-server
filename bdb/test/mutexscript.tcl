# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: mutexscript.tcl,v 11.12 2000/11/21 22:14:56 dda Exp $
#
# Random mutex tester.
# Usage: mutexscript dir numiters mlocks sleepint degree
# dir: dir in which all the mutexes live.
# numiters: Total number of iterations.
# nmutex: Total number of mutexes.
# sleepint: Maximum sleep interval.
# degree: Maximum number of locks to acquire at once

source ./include.tcl
source $test_path/test.tcl
source $test_path/testutils.tcl

set usage "mutexscript dir numiters nmutex sleepint degree"

# Verify usage
if { $argc != 5 } {
	puts stderr "FAIL:[timestamp] Usage: $usage"
	exit
}

# Initialize arguments
set dir [lindex $argv 0]
set numiters [ lindex $argv 1 ]
set nmutex [ lindex $argv 2 ]
set sleepint [ lindex $argv 3 ]
set degree [ lindex $argv 4 ]
set locker [pid]
set mypid [sanitized_pid]

# Initialize seed
global rand_init
berkdb srand $rand_init

puts -nonewline "Mutexscript: Beginning execution for $locker:"
puts " $numiters $nmutex $sleepint $degree"
flush stdout

# Open the environment and the mutex
set e [berkdb env -create -mode 0644 -lock -home $dir]
error_check_good evn_open [is_valid_env $e] TRUE

set mutex [$e mutex 0644 $nmutex]
error_check_good mutex_init [is_valid_mutex $mutex $e] TRUE

# Sleep for awhile to make sure that everyone has gotten in
tclsleep 5

for { set iter 0 } { $iter < $numiters } { incr iter } {
	set nlocks [berkdb random_int 1 $degree]
	# We will always lock objects in ascending order to avoid
	# deadlocks.
	set lastobj 1
	set mlist {}
	for { set lnum 0 } { $lnum < $nlocks } { incr lnum } {
		# Pick lock parameters
		set obj [berkdb random_int $lastobj [expr $nmutex - 1]]
		set lastobj [expr $obj + 1]
		puts "[timestamp] $locker $lnum: $obj"

		# Do get, set its val to own pid, and then add to list
		error_check_good mutex_get:$obj [$mutex get $obj] 0
		error_check_good mutex_setval:$obj [$mutex setval $obj $mypid] 0
		lappend mlist $obj
		if {$lastobj >= $nmutex} {
			break
		}
	}

	# Pick sleep interval
	tclsleep [ berkdb random_int 1 $sleepint ]

	# Now release locks
	foreach i $mlist {
		error_check_good mutex_getval:$i [$mutex getval $i] $mypid
		error_check_good mutex_setval:$i \
		    [$mutex setval $i [expr 0 - $mypid]] 0
		error_check_good mutex_release:$i [$mutex release $i] 0
	}
	puts "[timestamp] $locker released mutexes"
	flush stdout
}

puts "[timestamp] $locker Complete"
flush stdout
