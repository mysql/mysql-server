# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: env001.tcl,v 11.21 2000/11/09 19:24:08 sue Exp $
#
# Test of env remove interface.
proc env001 { } {
	global errorInfo
	global errorCode

	source ./include.tcl

	set testfile $testdir/env.db
	set t1 $testdir/t1

	puts "Env001: Test of environment remove interface."
	env_cleanup $testdir

	# Try opening without Create flag should error
	puts "\tEnv001.a: Open without create (should fail)."
	catch {set env [berkdb env -home $testdir]} ret
	error_check_good env:fail [is_substr $ret "no such file"] 1

	# Now try opening with create
	puts "\tEnv001.b: Open with create."
	set env [berkdb env -create -mode 0644 -home $testdir]
	error_check_bad env:$testdir $env NULL
	error_check_good env:$testdir [is_substr $env "env"] 1

	# Make sure that close works.
	puts "\tEnv001.c: Verify close."
	error_check_good env:close:$env [$env close] 0

	# Make sure we can reopen -- this doesn't work on Windows
	# because if there is only one opener, the region disappears
	# when it is closed.  We can't do a second opener, because
	# that will fail on HP-UX.
	puts "\tEnv001.d: Remove on closed environments."
	if { $is_windows_test != 1 } {
		puts "\t\tEnv001.d.1: Verify re-open."
		set env [berkdb env -home $testdir]
		error_check_bad env:$testdir $env NULL
		error_check_good env:$testdir [is_substr $env "env"] 1

		# remove environment
		puts "\t\tEnv001.d.2: Close environment."
		error_check_good env:close [$env close] 0
		puts "\t\tEnv001.d.3: Try remove with force (should succeed)."
		error_check_good \
		    envremove [berkdb envremove -force -home $testdir] 0
	}

	if { $is_windows_test != 1 && $is_hp_test != 1 } {
		puts "\tEnv001.e: Remove on open environments."
		puts "\t\tEnv001.e.1: Env is open by single proc,\
		    remove no force."
		set env [berkdb env -create -mode 0644 -home $testdir]
		error_check_bad env:$testdir $env NULL
		error_check_good env:$testdir [is_substr $env "env"] 1
		set stat [catch {berkdb envremove -home $testdir} ret]
		error_check_good env:remove $stat 1
		error_check_good env:close [$env close] 0
	}

	puts \
	    "\t\tEnv001.e.2: Env is open by single proc, remove with force."
	# Now that envremove doesn't do a close, this won't work on Windows.
	if { $is_windows_test != 1 && $is_hp_test != 1} {
		set env [berkdb env -create -mode 0644 -home $testdir]
		error_check_bad env:$testdir $env NULL
		error_check_good env:$testdir [is_substr $env "env"] 1
		set stat [catch {berkdb envremove -force -home $testdir} ret]
		error_check_good env:remove(force) $ret 0
		#
		# Even though the underlying env is gone, we need to close
		# the handle.
		#
		catch {$env close}
	}

	puts "\t\tEnv001.e.3: Env is open by 2 procs, remove no force."
	# should fail
	set env [berkdb env -create -mode 0644 -home $testdir]
	error_check_bad env:$testdir $env NULL
	error_check_good env:$testdir [is_substr $env "env"] 1

	set f1 [open |$tclsh_path r+]
	puts $f1 "source $test_path/test.tcl"

	set remote_env [send_cmd $f1 "berkdb env -home $testdir"]
	error_check_good remote:env_open [is_valid_env $remote_env] TRUE
	# First close our env, but leave remote open
	error_check_good env:close [$env close] 0
	catch {berkdb envremove -home $testdir} ret
	error_check_good envremove:2procs:noforce [is_substr $errorCode EBUSY] 1
	#
	# even though it failed, $env is no longer valid, so remove it in
	# the remote process
	set remote_close [send_cmd $f1 "$remote_env close"]
	error_check_good remote_close $remote_close 0

	# exit remote process
	set err [catch { close $f1 } result]
	error_check_good close_remote_process $err 0

	puts "\t\tEnv001.e.4: Env is open by 2 procs, remove with force."
	# You cannot do this on windows because you can't remove files that
	# are open, so we skip this test for Windows.  On UNIX, it should
	# succeed
	if { $is_windows_test != 1 && $is_hp_test != 1 } {
		set env [berkdb env -create -mode 0644 -home $testdir]
		error_check_bad env:$testdir $env NULL
		error_check_good env:$testdir [is_substr $env "env"] 1
		set f1 [open |$tclsh_path r+]
		puts $f1 "source $test_path/test.tcl"

		set remote_env [send_cmd $f1 "berkdb env -home $testdir"]
		error_check_good remote:env_open [is_valid_env $remote_env] TRUE

		catch {berkdb envremove -force -home $testdir} ret
		error_check_good envremove:2procs:force $ret 0
		#
		# We still need to close our handle.
		#
		catch {$env close} ret

		# Close down remote process
		set err [catch { close $f1 } result]
		error_check_good close_remote_process $err 0
	}

	# Try opening in a different dir
	puts "\tEnv001.f: Try opening env in another directory."
	if { [file exists $testdir/NEWDIR] != 1 } {
		file mkdir $testdir/NEWDIR
	}
	set eflags "-create -home $testdir/NEWDIR -mode 0644"
	set env [eval {berkdb env} $eflags]
	error_check_bad env:open $env NULL
	error_check_good env:close [$env close] 0
	error_check_good berkdb:envremove \
	    [berkdb envremove -home $testdir/NEWDIR] 0

	puts "\tEnv001 complete."
}
