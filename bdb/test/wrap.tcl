# Sentinel file wrapper for multi-process tests.
# This is designed to avoid a set of nasty bugs, primarily on Windows,
# where pid reuse causes watch_procs to sit around waiting for some
# random process that's not DB's and is not exiting.

source ./include.tcl

# Arguments:
#
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

# Set up argv for the subprocess, since the args aren't passed in as true
# arguments thanks to the pipe structure.
puts $t "set argc [llength $args]"
puts $t "set argv [list $args]"

# Command the test to run.
puts $t "source $test_path/$script"

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

exit $ret
