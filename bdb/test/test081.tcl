# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test081.tcl,v 11.3 2000/03/01 15:13:59 krinsky Exp $
#
# Test 81.
# Test off-page duplicates and overflow pages together with
# very large keys (key/data as file contents).
#
proc test081 { method {ndups 13} {tnum 81} args} {
	source ./include.tcl

	eval {test017 $method 1 $ndups $tnum} $args
}
