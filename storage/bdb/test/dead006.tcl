# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: dead006.tcl,v 1.4 2002/01/11 15:53:21 bostic Exp $
#
# TEST	dead006
# TEST	use timeouts rather than the normal dd algorithm.
proc dead006 { { procs "2 4 10" } {tests "ring clump" } \
    {timeout 1000} {tnum 006} } {
	source ./include.tcl

	dead001 $procs $tests $timeout $tnum
	dead002 $procs $tests $timeout $tnum
}
