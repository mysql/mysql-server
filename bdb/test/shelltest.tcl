# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: shelltest.tcl,v 1.20 2002/04/19 15:42:20 bostic Exp $
#
# TEST	scr###
# TEST	The scr### directories are shell scripts that test a variety of
# TEST	things, including things about the distribution itself.  These
# TEST	tests won't run on most systems, so don't even try to run them.
#
# shelltest.tcl:
#	Code to run shell script tests, to incorporate Java, C++,
#	example compilation, etc. test scripts into the Tcl framework.
proc shelltest { { run_one 0 }} {
	source ./include.tcl
	global shelltest_list

	set SH /bin/sh
	if { [file executable $SH] != 1 } {
		puts "Shell tests require valid shell /bin/sh: not found."
		puts "Skipping shell tests."
		return 0
	}

	if { $run_one == 0 } {
		puts "Running shell script tests..."

		foreach testpair $shelltest_list {
			set dir [lindex $testpair 0]
			set test [lindex $testpair 1]

			env_cleanup $testdir
			shelltest_copy $test_path/$dir $testdir
			shelltest_run $SH $dir $test $testdir
		}
	} else {
		set run_one [expr $run_one - 1];
		set dir [lindex [lindex $shelltest_list $run_one] 0]
		set test [lindex [lindex $shelltest_list $run_one] 1]

		env_cleanup $testdir
		shelltest_copy $test_path/$dir $testdir
		shelltest_run $SH $dir $test $testdir
	}
}

proc shelltest_copy { fromdir todir } {
	set globall [glob $fromdir/*]

	foreach f $globall {
		file copy $f $todir/
	}
}

proc shelltest_run { sh srcdir test testdir } {
	puts "Running shell script $srcdir ($test)..."

	set ret [catch {exec $sh -c "cd $testdir && sh $test" >&@ stdout} res]

	if { $ret != 0 } {
		puts "FAIL: shell test $srcdir/$test exited abnormally"
	}
}

proc scr001 {} { shelltest 1 }
proc scr002 {} { shelltest 2 }
proc scr003 {} { shelltest 3 }
proc scr004 {} { shelltest 4 }
proc scr005 {} { shelltest 5 }
proc scr006 {} { shelltest 6 }
proc scr007 {} { shelltest 7 }
proc scr008 {} { shelltest 8 }
proc scr009 {} { shelltest 9 }
proc scr010 {} { shelltest 10 }
proc scr011 {} { shelltest 11 }
proc scr012 {} { shelltest 12 }
proc scr013 {} { shelltest 13 }
proc scr014 {} { shelltest 14 }
proc scr015 {} { shelltest 15 }
proc scr016 {} { shelltest 16 }
proc scr017 {} { shelltest 17 }
proc scr018 {} { shelltest 18 }
proc scr019 {} { shelltest 19 }
proc scr020 {} { shelltest 20 }
proc scr021 {} { shelltest 21 }
proc scr022 {} { shelltest 22 }
