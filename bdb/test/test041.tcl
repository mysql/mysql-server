# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test041.tcl,v 11.3 2000/02/14 03:00:20 bostic Exp $
#
# DB Test 41 {access method}
# DB_GET_BOTH functionality with off-page duplicates.
proc test041 { method {nentries 10000} args} {
	# Test with off-page duplicates
	eval {test039 $method $nentries 20 41 -pagesize 512} $args

	# Test with multiple pages of off-page duplicates
	eval {test039 $method [expr $nentries / 10] 100 41 -pagesize 512} $args
}
