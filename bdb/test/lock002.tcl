# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: lock002.tcl,v 11.10 2000/08/25 14:21:51 sue Exp $
#
# Exercise basic multi-process aspects of lock.
proc lock002 { {maxlocks 1000} {conflicts {0 0 0 0 0 1 0 1 1} } } {
	source ./include.tcl

	puts "Lock002: Basic multi-process lock tests."

	env_cleanup $testdir

	set nmodes [isqrt [llength $conflicts]]

	# Open the lock
	mlock_open $maxlocks $nmodes $conflicts
	mlock_wait
}

# Make sure that we can create a region; destroy it, attach to it,
# detach from it, etc.
proc mlock_open { maxl nmodes conflicts } {
	source ./include.tcl

	puts "Lock002.a multi-process open/close test"

	# Open/Create region here.  Then close it and try to open from
	# other test process.
	set env_cmd [concat "berkdb env -create -mode 0644 \
	    -lock -lock_max $maxl -lock_conflict" \
	    [list [list $nmodes $conflicts]] "-home $testdir"]
	set local_env [eval $env_cmd]
	error_check_good env_open [is_valid_env $local_env] TRUE

	set ret [$local_env close]
	error_check_good env_close $ret 0

	# Open from other test process
	set env_cmd "berkdb env -mode 0644 -home $testdir"

	set f1 [open |$tclsh_path r+]
	puts $f1 "source $test_path/test.tcl"

	set remote_env [send_cmd $f1 $env_cmd]
	error_check_good remote:env_open [is_valid_env $remote_env] TRUE

	# Now make sure that we can reopen the region.
	set local_env [eval $env_cmd]
	error_check_good env_open [is_valid_env $local_env] TRUE
	set ret [$local_env close]
	error_check_good env_close $ret 0

	# Try closing the remote region
	set ret [send_cmd $f1 "$remote_env close"]
	error_check_good remote:lock_close $ret 0

	# Try opening for create.  Will succeed because region exists.
	set env_cmd [concat "berkdb env -create -mode 0644 \
	    -lock -lock_max $maxl -lock_conflict" \
	    [list [list $nmodes $conflicts]] "-home $testdir"]
	set local_env [eval $env_cmd]
	error_check_good remote:env_open [is_valid_env $local_env] TRUE

	# close locally
	reset_env $local_env

	# Close and exit remote
	set ret [send_cmd $f1 "reset_env $remote_env"]

	catch { close $f1 } result
}

proc mlock_wait { } {
	source ./include.tcl

	puts "Lock002.b multi-process get/put wait test"

	# Open region locally
	set env_cmd "berkdb env -lock -home $testdir"
	set local_env [eval $env_cmd]
	error_check_good env_open [is_valid_env $local_env] TRUE

	# Open region remotely
	set f1 [open |$tclsh_path r+]

	puts $f1 "source $test_path/test.tcl"

	set remote_env [send_cmd $f1 $env_cmd]
	error_check_good remote:env_open [is_valid_env $remote_env] TRUE

	# Get a write lock locally; try for the read lock
	# remotely.  We hold the locks for several seconds
	# so that we can use timestamps to figure out if the
	# other process waited.
	set locker 1
	set local_lock [$local_env lock_get write $locker object1]
	error_check_good lock_get [is_valid_lock $local_lock $local_env] TRUE

	# Now request a lock that we expect to hang; generate
	# timestamps so we can tell if it actually hangs.
	set locker 2
	set remote_lock [send_timed_cmd $f1 1 \
	    "set lock \[$remote_env lock_get write $locker object1\]"]

	# Now sleep before releasing lock
	tclsleep 5
	set result [$local_lock put]
	error_check_good lock_put $result 0

	# Now get the result from the other script
	set result [rcv_result $f1]
	error_check_good lock_get:remote_time [expr $result > 4] 1

	# Now get the remote lock
	set remote_lock [send_cmd $f1 "puts \$lock"]
	error_check_good remote:lock_get \
	    [is_valid_lock $remote_lock $remote_env] TRUE

	# Now make the other guy wait 5 second and then release his
	# lock while we try to get a write lock on it
	set start [timestamp -r]

	set ret [send_cmd $f1 "tclsleep 5"]

	set ret [send_cmd $f1 "$remote_lock put"]

	set locker 1
	set local_lock [$local_env lock_get write $locker object1]
	error_check_good lock_get:time \
	    [expr [expr [timestamp -r] - $start] > 2] 1
	error_check_good lock_get:local \
	    [is_valid_lock $local_lock $local_env] TRUE

	# Now check remote's result
	set result [rcv_result $f1]
	error_check_good lock_put:remote $result 0

	# Clean up remote
	set ret [send_cmd $f1 "reset_env $remote_env"]

	close $f1

	# Now close up locally
	set ret [$local_lock put]
	error_check_good lock_put $ret 0

	reset_env $local_env
}
