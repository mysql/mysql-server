# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: dead007.tcl,v 1.3 2002/01/11 15:53:22 bostic Exp $
#
# TEST	dead007
# TEST	use timeouts rather than the normal dd algorithm.
proc dead007 { } {
	source ./include.tcl
	global lock_curid
	global lock_maxid

	set save_curid $lock_curid
	set save_maxid $lock_maxid
	puts "Dead007.a -- wrap around"
	set lock_curid [expr $lock_maxid - 2]
	dead001 "2 10"
	## Oldest/youngest breaks when the id wraps
	# dead003 "4 10"
	dead004

	puts "Dead007.b -- extend space"
	set lock_maxid [expr $lock_maxid - 3]
	set lock_curid [expr $lock_maxid - 1]
	dead001 "4 10"
	## Oldest/youngest breaks when the id wraps
	# dead003 "10"
	dead004

	set lock_curid $save_curid
	set lock_maxid $save_maxid
}
