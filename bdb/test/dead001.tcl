# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: dead001.tcl,v 11.17 2000/11/05 14:23:55 dda Exp $
#
# Deadlock Test 1.
# We create various deadlock scenarios for different numbers of lockers
# and see if we can get the world cleaned up suitably.
proc dead001 { { procs "2 4 10" } {tests "ring clump" } } {
	source ./include.tcl

	puts "Dead001: Deadlock detector tests"

	env_cleanup $testdir

	# Create the environment.
	puts "\tDead001.a: creating environment"
	set env [berkdb env -create -mode 0644 -lock -home $testdir]
	error_check_good lock_env:open [is_valid_env $env] TRUE

	error_check_good lock_env:close [$env close] 0

	set dpid [exec $util_path/db_deadlock -vw -h $testdir \
	    >& $testdir/dd.out &]

	foreach t $tests {
		set pidlist ""
		foreach n $procs {

			sentinel_init 

			# Fire off the tests
			puts "\tDead001: $n procs of test $t"
			for { set i 0 } { $i < $n } { incr i } {
				puts "$tclsh_path $test_path/wrap.tcl \
				    $testdir/dead001.log.$i \
				    ddscript.tcl $testdir $t $i $i $n"
				set p [exec $tclsh_path \
					$test_path/wrap.tcl \
					ddscript.tcl $testdir/dead001.log.$i \
					$testdir $t $i $i $n &]
				lappend pidlist $p
			}
			watch_procs 5

			# Now check output
			set dead 0
			set clean 0
			set other 0
			for { set i 0 } { $i < $n } { incr i } {
				set did [open $testdir/dead001.log.$i]
				while { [gets $did val] != -1 } {
					switch $val {
						DEADLOCK { incr dead }
						1 { incr clean }
						default { incr other }
					}
				}
				close $did
			}
			puts "dead check..."
			dead_check $t $n $dead $clean $other
		}
	}

	exec $KILL $dpid
	# Windows needs files closed before deleting files, so pause a little
	tclsleep 2
	fileremove -f $testdir/dd.out
	# Remove log files
	for { set i 0 } { $i < $n } { incr i } {
		fileremove -f $testdir/dead001.log.$i
	}
}
