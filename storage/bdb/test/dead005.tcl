# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: dead005.tcl,v 11.10 2002/09/05 17:23:05 sandstro Exp $
#
# Deadlock Test 5.
# Test out the minlocks, maxlocks, and minwrites options
# to the deadlock detector.
proc dead005 { { procs "4 6 10" } {tests "maxlocks minwrites minlocks" } } {
	source ./include.tcl

	puts "Dead005: minlocks, maxlocks, and minwrites deadlock detection tests"
	foreach t $tests {
		puts "Dead005.$t: creating environment"
		env_cleanup $testdir

		# Create the environment.
		set env [berkdb_env -create -mode 0644 -lock -home $testdir]
		error_check_good lock_env:open [is_valid_env $env] TRUE
		case $t {
			minlocks { set to n }
			maxlocks { set to m }
			minwrites { set to w }
		}
		foreach n $procs {
			set dpid [exec $util_path/db_deadlock -vw -h $testdir \
			    -a $to >& $testdir/dd.out &]
			sentinel_init
			set pidlist ""

			# Fire off the tests
			puts "\tDead005: $t test with $n procs"
			for { set i 0 } { $i < $n } { incr i } {
				set locker [$env lock_id]
				puts "$tclsh_path $test_path/wrap.tcl \
				    $testdir/dead005.log.$i \
				    ddscript.tcl $testdir $t $locker $i $n"
				set p [exec $tclsh_path \
					$test_path/wrap.tcl \
					ddscript.tcl $testdir/dead005.log.$i \
					$testdir $t $locker $i $n &]
				lappend pidlist $p
			}
			watch_procs $pidlist 5

			# Now check output
			set dead 0
			set clean 0
			set other 0
			for { set i 0 } { $i < $n } { incr i } {
				set did [open $testdir/dead005.log.$i]
				while { [gets $did val] != -1 } {
					switch $val {
						DEADLOCK { incr dead }
						1 { incr clean }
						default { incr other }
					}
				}
				close $did
			}
			tclkill $dpid
			puts "dead check..."
			dead_check $t $n 0 $dead $clean $other
			# Now verify that the correct participant
			# got deadlocked.
			switch $t {
				minlocks {set f 0}
				minwrites {set f 1}
				maxlocks {set f [expr $n - 1]}
			}
			set did [open $testdir/dead005.log.$f]
			error_check_bad file:$t [gets $did val] -1
			error_check_good read($f):$t $val DEADLOCK
			close $did
		}
		error_check_good lock_env:close [$env close] 0
		# Windows needs files closed before deleting them, so pause
		tclsleep 2
		fileremove -f $testdir/dd.out
		# Remove log files
		for { set i 0 } { $i < $n } { incr i } {
			fileremove -f $testdir/dead001.log.$i
		}
	}
}
