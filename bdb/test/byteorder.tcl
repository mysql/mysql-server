# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: byteorder.tcl,v 11.12 2002/07/29 18:09:25 sue Exp $
#
# Byte Order Test
# Use existing tests and run with both byte orders.
proc byteorder { method {nentries 1000} } {
	source ./include.tcl
	puts "Byteorder: $method $nentries"

	eval {test001 $method $nentries 0 "01" 0 -lorder 1234}
	eval {verify_dir $testdir}
	eval {test001 $method $nentries 0 "01" 0 -lorder 4321}
	eval {verify_dir $testdir}
	eval {test003 $method -lorder 1234}
	eval {verify_dir $testdir}
	eval {test003 $method -lorder 4321}
	eval {verify_dir $testdir}
	eval {test010 $method $nentries 5 10 -lorder 1234}
	eval {verify_dir $testdir}
	eval {test010 $method $nentries 5 10 -lorder 4321}
	eval {verify_dir $testdir}
	eval {test011 $method $nentries 5 11 -lorder 1234}
	eval {verify_dir $testdir}
	eval {test011 $method $nentries 5 11 -lorder 4321}
	eval {verify_dir $testdir}
	eval {test018 $method $nentries -lorder 1234}
	eval {verify_dir $testdir}
	eval {test018 $method $nentries -lorder 4321}
	eval {verify_dir $testdir}
}
