# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test069.tcl,v 11.4 2000/02/14 03:00:21 bostic Exp $
#
# DB Test 69: Run DB Test 67 with a small number of dups,
# to ensure that partial puts to DB_CURRENT work correctly in
# the absence of duplicate pages.

proc test069 { method {ndups 50} {tnum 69} args } {
	eval test067 $method $ndups $tnum $args
}
