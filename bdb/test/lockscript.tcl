# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: lockscript.tcl,v 11.11 2000/03/24 19:53:39 krinsky Exp $
#
# Random lock tester.
# Usage: lockscript dir numiters numobjs sleepint degree readratio
# dir: lock directory.
# numiters: Total number of iterations.
# numobjs: Number of objects on which to lock.
# sleepint: Maximum sleep interval.
# degree: Maximum number of locks to acquire at once
# readratio: Percent of locks that should be reads.

source ./include.tcl
source $test_path/test.tcl

set usage "lockscript dir numiters numobjs sleepint degree readratio"

# Verify usage
if { $argc != 6 } {
	puts stderr "FAIL:[timestamp] Usage: $usage"
	exit
}

# Initialize arguments
set dir [lindex $argv 0]
set numiters [ lindex $argv 1 ]
set numobjs [ lindex $argv 2 ]
set sleepint [ lindex $argv 3 ]
set degree [ lindex $argv 4 ]
set readratio [ lindex $argv 5 ]
set locker [pid]

# Initialize random number generator
global rand_init
berkdb srand $rand_init

puts -nonewline "Beginning execution for $locker: $numiters $numobjs "
puts "$sleepint $degree $readratio"
flush stdout

set e [berkdb env -create -lock -home $dir]
error_check_good env_open [is_substr $e env] 1

for { set iter 0 } { $iter < $numiters } { incr iter } {
	set nlocks [berkdb random_int 1 $degree]
	# We will always lock objects in ascending order to avoid
	# deadlocks.
	set lastobj 1
	set locklist {}
	for { set lnum 0 } { $lnum < $nlocks } { incr lnum } {
		# Pick lock parameters
		set obj [berkdb random_int $lastobj $numobjs]
		set lastobj [expr $obj + 1]
		set x [berkdb random_int 1 100 ]
		if { $x <= $readratio } {
			set rw read
		} else {
			set rw write
		}
		puts "[timestamp] $locker $lnum: $rw $obj"

		# Do get; add to list
		set lockp [$e lock_get $rw $locker $obj]
		lappend locklist $lockp
		if {$lastobj > $numobjs} {
			break
		}
	}
	# Pick sleep interval
	tclsleep [berkdb random_int 1 $sleepint]

	# Now release locks
	puts "[timestamp] $locker released locks"
	release_list $locklist
	flush stdout
}

set ret [$e close]
error_check_good env_close $ret 0

puts "[timestamp] $locker Complete"
flush stdout

exit
