# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: mutex002.tcl,v 11.25 2004/01/28 03:36:28 bostic Exp $
#

# TEST	mutex002
# TEST	Test basic mutex synchronization
proc mutex002 { } {
	source ./include.tcl

	puts "Mutex002: Basic synchronization"
	env_cleanup $testdir
	set nlocks 20

	# Fork off child before we open any files.
	set f1 [open |$tclsh_path r+]
	puts $f1 "source $test_path/test.tcl"
	flush $f1

	# Open the environment and the mutex locally
	puts "\tMutex002.a: Open local and remote env"
	set local_env [berkdb_env -create -mode 0644 -lock -home $testdir]
	error_check_good env_open [is_valid_env $local_env] TRUE

	set local_mutex [$local_env mutex 0644 $nlocks]
	error_check_good \
	    mutex_init [is_valid_mutex $local_mutex $local_env] TRUE

	# Open the environment and the mutex remotely
	set remote_env [send_cmd $f1 "berkdb_env -lock -home $testdir"]
	error_check_good remote:env_open [is_valid_env $remote_env] TRUE

	set remote_mutex [send_cmd $f1 "$remote_env mutex 0644 $nlocks"]
	error_check_good \
	    mutex_init [is_valid_mutex $remote_mutex $remote_env] TRUE

	# Do a get here, then set the value to be pid.
	# On the remote side fire off a get and getval.
	puts "\tMutex002.b: Local and remote get/set"
	set r [$local_mutex get 1]
	error_check_good lock_get $r 0

	set r [$local_mutex setval 1 [pid]]
	error_check_good lock_get $r 0

	# Now have the remote side request the lock and check its
	# value. Then wait 5 seconds, release the mutex and see
	# what the remote side returned.
	send_timed_cmd $f1 1 "$remote_mutex get 1"
	send_timed_cmd $f1 1 "set ret \[$remote_mutex getval 1\]"

	# Now sleep before resetting and releasing lock
	tclsleep 5
	set newv [expr [pid] - 1]
	set r [$local_mutex setval 1 $newv]
	error_check_good mutex_setval $r 0

	set r [$local_mutex release 1]
	error_check_good mutex_release $r 0

	# Now get the result from the other script
	# Timestamp
	set result [rcv_result $f1]
	error_check_good lock_get:remote_time [expr $result > 4] 1

	# Timestamp
	set result [rcv_result $f1]

	# Mutex value
	set result [send_cmd $f1 "puts \$ret"]
	error_check_good lock_get:remote_getval $result $newv

	# Close down the remote
	puts "\tMutex002.c: Close remote"
	set ret [send_cmd $f1 "$remote_mutex close" 5]
	# Not sure why we need this, but we do... an extra blank line
	# someone gets output somewhere
	gets $f1 ret
	error_check_good remote:mutex_close $ret 0

	set ret [send_cmd $f1 "$remote_env close"]
	error_check_good remote:env_close $ret 0

	catch { close $f1 } result

	set ret [$local_mutex close]
	error_check_good local:mutex_close $ret 0

	set ret [$local_env close]
	error_check_good local:env_close $ret 0
}
