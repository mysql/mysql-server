# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: lock001.tcl,v 11.11 2000/08/25 14:21:51 sue Exp $
#
# Test driver for lock tests.
#						General	Multi	Random
# Options are:
# -dir <directory in which to store mpool>	Y	Y	Y
# -iterations <iterations>			Y	N	Y
# -ldegree <number of locks per iteration>	N	N	Y
# -maxlocks <locks in table>			Y	Y	Y
# -objs <number of objects>			N	N	Y
# -procs <number of processes to run>		N	N	Y
# -reads <read ratio>				N	N	Y
# -seeds <list of seed values for processes>	N	N	Y
# -wait <wait interval after getting locks>	N	N	Y
# -conflicts <conflict matrix; a list of lists>	Y	Y	Y
proc lock_usage {} {
	puts stderr "randomlock\n\t-dir <dir>\n\t-iterations <iterations>"
	puts stderr "\t-conflicts <conflict matrix>"
	puts stderr "\t-ldegree <locks per iteration>\n\t-maxlocks <n>"
	puts stderr "\t-objs <objects>\n\t-procs <nprocs>\n\t-reads <%reads>"
	puts stderr "\t-seeds <list of seeds>\n\t-wait <max wait interval>"
	return
}

proc locktest { args } {
	source ./include.tcl

	# Set defaults
	# Adjusted to make exact match of isqrt
	#set conflicts { 3 0 0 0 0 0 1 0 1 1}
	#set conflicts { 3 0 0 0 0 1 0 1 1}
	set conflicts { 0 0 0 0 0 1 0 1 1}
	set iterations 1000
	set ldegree 5
	set maxlocks 1000
	set objs 75
	set procs 5
	set reads 65
	set seeds {}
	set wait 5
	for { set i 0 } { $i < [llength $args] } {incr i} {
		switch -regexp -- [lindex $args $i] {
			-c.* { incr i; set conflicts [linkdex $args $i] }
			-d.* { incr i; set testdir [lindex $args $i] }
			-i.* { incr i; set iterations [lindex $args $i] }
			-l.* { incr i; set ldegree [lindex $args $i] }
			-m.* { incr i; set maxlocks [lindex $args $i] }
			-o.* { incr i; set objs [lindex $args $i] }
			-p.* { incr i; set procs [lindex $args $i] }
			-r.* { incr i; set reads [lindex $args $i] }
			-s.* { incr i; set seeds [lindex $args $i] }
			-w.* { incr i; set wait [lindex $args $i] }
			default {
				puts -nonewline "FAIL:[timestamp] Usage: "
				lock_usage
				return
			}
		}
	}
	set nmodes [isqrt [llength $conflicts]]

	# Cleanup
	env_cleanup $testdir

	# Open the region we'll use for testing.
	set eflags "-create -lock -home $testdir -mode 0644 \
	    -lock_max $maxlocks -lock_conflict {$nmodes {$conflicts}}"
	set env [eval {berkdb env} $eflags]
	lock001 $env $iterations $nmodes
	reset_env $env
	env_cleanup $testdir

	lock002 $maxlocks $conflicts

	lock003 $testdir $iterations \
	    $maxlocks $procs $ldegree $objs $reads $wait $conflicts $seeds
}

# Make sure that the basic lock tests work.  Do some simple gets and puts for
# a single locker.
proc lock001 {env iter nmodes} {
	source ./include.tcl

	puts "Lock001: test basic lock operations"
	set locker 999
	# Get and release each type of lock
	puts "Lock001.a: get and release each type of lock"
	foreach m {ng write read} {
		set obj obj$m
		set lockp [$env lock_get $m $locker $obj]
		error_check_good lock_get:a [is_blocked $lockp] 0
		error_check_good lock_get:a [is_substr $lockp $env] 1
		set ret [ $lockp put ]
		error_check_good lock_put $ret 0
	}

	# Get a bunch of locks for the same locker; these should work
	set obj OBJECT
	puts "Lock001.b: Get a bunch of locks for the same locker"
	foreach m {ng write read} {
		set lockp [$env lock_get $m $locker $obj ]
		lappend locklist $lockp
		error_check_good lock_get:b [is_blocked $lockp] 0
		error_check_good lock_get:b [is_substr $lockp $env] 1
	}
	release_list $locklist

	set locklist {}
	# Check that reference counted locks work
	puts "Lock001.c: reference counted locks."
	for {set i 0} { $i < 10 } {incr i} {
		set lockp [$env lock_get -nowait write $locker $obj]
		error_check_good lock_get:c [is_blocked $lockp] 0
		error_check_good lock_get:c [is_substr $lockp $env] 1
		lappend locklist $lockp
	}
	release_list $locklist

	# Finally try some failing locks
	set locklist {}
	foreach i {ng write read} {
		set lockp [$env lock_get $i $locker $obj]
		lappend locklist $lockp
		error_check_good lock_get:d [is_blocked $lockp] 0
		error_check_good lock_get:d [is_substr $lockp $env] 1
	}

	# Change the locker
	set locker [incr locker]
	set blocklist {}
	# Skip NO_LOCK lock.
	puts "Lock001.e: Change the locker, acquire read and write."
	foreach i {write read} {
		catch {$env lock_get -nowait $i $locker $obj} ret
		error_check_good lock_get:e [is_substr $ret "not granted"] 1
		#error_check_good lock_get:e [is_substr $lockp $env] 1
		#error_check_good lock_get:e [is_blocked $lockp] 0
	}
	# Now release original locks
	release_list $locklist

	# Now re-acquire blocking locks
	set locklist {}
	puts "Lock001.f: Re-acquire blocking locks."
	foreach i {write read} {
		set lockp [$env lock_get -nowait $i $locker $obj ]
		error_check_good lock_get:f [is_substr $lockp $env] 1
		error_check_good lock_get:f [is_blocked $lockp] 0
		lappend locklist $lockp
	}

	# Now release new locks
	release_list $locklist

	puts "Lock001 Complete."
}

# Blocked locks appear as lockmgrN.lockM\nBLOCKED
proc is_blocked { l } {
	if { [string compare $l BLOCKED ] == 0 } {
		return 1
	} else {
		return 0
	}
}
