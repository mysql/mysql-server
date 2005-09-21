# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: wrap.tcl,v 11.8 2004/01/28 03:36:33 bostic Exp $
#
# Sentinel file wrapper for multi-process tests.  This is designed to avoid a
# set of nasty bugs, primarily on Windows, where pid reuse causes watch_procs
# to sit around waiting for some random process that's not DB's and is not
# exiting.

source ./include.tcl
source $test_path/testutils.tcl

# Arguments:
if { $argc < 3 } {
	puts "FAIL: wrap.tcl: Usage: wrap.tcl script log scriptargs"
	exit
}

set script [lindex $argv 0]
set logfile [lindex $argv 1]
set args [lrange $argv 2 end]

# Create a sentinel file to mark our creation and signal that watch_procs
# should look for us.
set parentpid [pid]
set parentsentinel $testdir/begin.$parentpid
set f [open $parentsentinel w]
close $f

# Create a Tcl subprocess that will actually run the test.
set t [open "|$tclsh_path >& $logfile" w]

# Create a sentinel for the subprocess.
set childpid [pid $t]
puts "Script watcher process $parentpid launching $script process $childpid."
set childsentinel $testdir/begin.$childpid
set f [open $childsentinel w]
close $f

puts $t "source $test_path/test.tcl"
puts $t "set script $script"

# Set up argv for the subprocess, since the args aren't passed in as true
# arguments thanks to the pipe structure.
puts $t "set argc [llength $args]"
puts $t "set argv [list $args]"

puts $t {set ret [catch { source $test_path/$script } result]}
puts $t {if { [string length $result] > 0 } { puts $result }}
puts $t {error_check_good "$test_path/$script run: pid [pid]" $ret 0}

# Close the pipe.  This will flush the above commands and actually run the
# test, and will also return an error a la exec if anything bad happens
# to the subprocess.  The magic here is that closing a pipe blocks
# and waits for the exit of processes in the pipeline, at least according
# to Ousterhout (p. 115).

set ret [catch {close $t} res]

# Write ending sentinel files--we're done.
set f [open $testdir/end.$childpid w]
close $f
set f [open $testdir/end.$parentpid w]
close $f

error_check_good "Pipe close ($childpid: $script $argv: logfile $logfile)"\
    $ret 0
exit $ret
