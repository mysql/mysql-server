# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: test081.tcl,v 11.6 2002/01/11 15:53:55 bostic Exp $
#
# TEST	test081
# TEST	Test off-page duplicates and overflow pages together with
# TEST	very large keys (key/data as file contents).
proc test081 { method {ndups 13} {tnum 81} args} {
	source ./include.tcl

	eval {test017 $method 1 $ndups $tnum} $args
}
