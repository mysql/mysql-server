# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: dead003.tcl,v 1.8 2000/08/25 14:21:50 sue Exp $
#
# Deadlock Test 3.
# Test DB_LOCK_OLDEST and DB_LOCK_YOUNGEST
# Identical to Test 2 except that we create the region with "detect on
# every wait" with first the "oldest" and then "youngest".
proc dead003 { { procs "2 4 10" } {tests "ring clump" } } {
	source ./include.tcl

	set detects { oldest youngest }
	puts "Dead003: Deadlock detector tests: $detects"

	# Create the environment.
	foreach d $detects {
		env_cleanup $testdir
		puts "\tDead003.a: creating environment for $d"
		set env [berkdb env \
		    -create -mode 0644 -home $testdir -lock -lock_detect $d]
		error_check_good lock_env:open [is_valid_env $env] TRUE
		error_check_good lock_env:close [$env close] 0

		foreach t $tests {
			set pidlist ""
			foreach n $procs {
				sentinel_init 

				# Fire off the tests
				puts "\tDead003: $n procs of test $t"
				for { set i 0 } { $i < $n } { incr i } {
					puts "$tclsh_path\
					    test_path/ddscript.tcl $testdir \
					    $t $i $i $n >& \
					    $testdir/dead003.log.$i"
					set p [exec $tclsh_path \
					    $test_path/wrap.tcl \
					    ddscript.tcl \
					    $testdir/dead003.log.$i $testdir \
					    $t $i $i $n &]
					lappend pidlist $p
				}
				watch_procs 5

				# Now check output
				set dead 0
				set clean 0
				set other 0
				for { set i 0 } { $i < $n } { incr i } {
					set did [open $testdir/dead003.log.$i]
					while { [gets $did val] != -1 } {
						switch $val {
							DEADLOCK { incr dead }
							1 { incr clean }
							default { incr other }
						}
					}
					close $did
				}
				dead_check $t $n $dead $clean $other
				#
				# If we get here we know we have the
				# correct number of dead/clean procs, as
				# checked by dead_check above.  Now verify
				# that the right process was the one.
				puts "\tDead003: Verify $d locks were aborted"
				set l ""
				if { $d == "oldest" } {
					set l [expr $n - 1]
				}
				if { $d == "youngest" } {
					set l 0
				}
				set did [open $testdir/dead003.log.$l]
				while { [gets $did val] != -1 } {
					error_check_good check_abort \
					    $val 1
				}
				close $did
			}
		}

		fileremove -f $testdir/dd.out
		# Remove log files
		for { set i 0 } { $i < $n } { incr i } {
			fileremove -f $testdir/dead003.log.$i
		}
	}
}
